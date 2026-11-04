//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "mhExtGpuStreams.h"
#include "mhExtGpuBufferReadback.h"

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mhExtGpuBufferBridge.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/profilingUtils.h>

#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/hd/externalBuffer.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hgi/externalBuffer.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <cinttypes> // PRIu64, for the GPU buffer sharing debug output
#include <cstring> // memcpy; memcpy_s on MSVC
#include <limits>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

TF_DEFINE_ENV_SETTING(MAYAHYDRA_GPU_BUFFER_SHARING, true,
    "Enable GPU buffer sharing between VP2 and Hydra Storm");

TF_DEFINE_ENV_SETTING(MAYAHYDRA_GPU_BUFFER_SHARING_MODE, "hybrid",
    "GPU buffer sharing mode when MAYAHYDRA_GPU_BUFFER_SHARING is enabled. "
    "Accepted values: 'direct' (zero-copy direct binding, non-batchable), "
    "'batch' (GPU-to-GPU blit into Storm's aggregated VBOs to enable "
    "indirect-draw batching at the cost of one blit per dirty primvar per "
    "frame for animating meshes), or 'hybrid' (default; start in batch so "
    "static meshes batch from frame one, then sticky-promote any mesh that "
    "ever fires MVS_changedGeometry / MVS_changedTopo to direct mode "
    "permanently for zero per-frame copy cost on animating meshes).");

constexpr uint8_t kLazyCpuPointsBit = 1u << 0;
constexpr uint8_t kLazyCpuNormalsBit = 1u << 1;
constexpr uint8_t kLazyCpuTangentsBit = 1u << 2;
constexpr uint8_t kLazyCpuUvsBit = 1u << 3;

#define TF_DEBUG_GPU_BUFFER_SHARING(fmt, ...)                      \
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)      \
        .Msg("[GPU Buffer Sharing]: " fmt, ##__VA_ARGS__)

namespace {

enum class _GpuBufferSharingMode {
    Direct,
    Batch,
    Hybrid, // start in batch, sticky-promote to direct on first observed deform
};

// Cached once at startup. Unrecognized values fall back to Hybrid (default).
_GpuBufferSharingMode _GetGpuBufferSharingMode()
{
    static const _GpuBufferSharingMode mode = []() {
        const std::string raw = TfGetEnvSetting(MAYAHYDRA_GPU_BUFFER_SHARING_MODE);
        if (raw == "direct") {
            return _GpuBufferSharingMode::Direct;
        }
        if (raw == "batch") {
            return _GpuBufferSharingMode::Batch;
        }
        return _GpuBufferSharingMode::Hybrid;
    }();
    return mode;
}

static uint64_t
_ExtractRawHandle(void *resourceHandle)
{
    // resourceHandle() returns a pointer-to-the-native-handle, not the handle
    // itself. For GL the native handle is a GLuint (4 bytes); for Vulkan/Metal
    // it is an 8-byte opaque handle/pointer.
#if defined(__APPLE__)
    return reinterpret_cast<uint64_t>(*static_cast<void **>(resourceHandle));
#elif defined(_WIN32) || defined(__linux__)
    return static_cast<uint64_t>(*static_cast<unsigned int *>(resourceHandle));
#else
    return 0;
#endif
}

static HdTupleType
_ToHdTupleType(MHWRender::MGeometry::DataType dataType, int dimension)
{
    HdType hdType = HdTypeInvalid;
    switch (dataType) {
    case MHWRender::MGeometry::kFloat:
        switch (dimension) {
        case 1: hdType = HdTypeFloat;     break;
        case 2: hdType = HdTypeFloatVec2; break;
        case 3: hdType = HdTypeFloatVec3; break;
        case 4: hdType = HdTypeFloatVec4; break;
        }
        break;
    case MHWRender::MGeometry::kInt32:
        switch (dimension) {
        case 1: hdType = HdTypeInt32;     break;
        case 2: hdType = HdTypeInt32Vec2; break;
        case 3: hdType = HdTypeInt32Vec3; break;
        case 4: hdType = HdTypeInt32Vec4; break;
        }
        break;
    case MHWRender::MGeometry::kDouble:
        switch (dimension) {
        case 1: hdType = HdTypeDouble;     break;
        case 2: hdType = HdTypeDoubleVec2; break;
        case 3: hdType = HdTypeDoubleVec3; break;
        case 4: hdType = HdTypeDoubleVec4; break;
        }
        break;
    default:
        break;
    }
    return { hdType, 1 };
}

// The byte layout of one VP2 vertex buffer stream, plus the smallest
// allocation size that provably contains it. The returned source names no
// buffer yet (rawHandle 0); a layout Hydra cannot describe comes back with no
// element type or no elements.
static MhExtGpuSource
_LayoutFromDescriptor(const MHWRender::MVertexBufferDescriptor &desc, size_t vertexCount)
{
    MhExtGpuSource layout;
    layout.elementType = _ToHdTupleType(desc.dataType(), desc.dimension());
    if (layout.elementType.type == HdTypeInvalid) {
        return layout;
    }
    if (desc.offset() < 0 || desc.stride() < 0) {
        return {};
    }
    layout.numElements = vertexCount;
    if (layout.numElements == 0) {
        return {};
    }

    // offset() and stride() are both counted in dataType units, not bytes.
    layout.byteOffset =
        static_cast<size_t>(desc.offset()) * desc.dataTypeSize();
    layout.byteStride =
        static_cast<size_t>(desc.stride()) * desc.dataTypeSize();

    // The arena needs a size for the buffer it registers, and the consumer
    // bounds checks the published layout against it
    // (HdStExtGpuBufferDesc::FromSchema) -- so an underestimate does not fail
    // loudly, it silently drops the stream back to the CPU path. MVertexBuffer
    // exposes no allocation size, so derive the smallest size that covers the
    // stream, applying the same stride rule the consumer does: a zero stride
    // means tightly packed.
    const size_t elemSize = HdDataSizeOfTupleType(layout.elementType);
    if (layout.byteStride > 0 && layout.byteStride < elemSize) {
        return {};
    }
    const size_t stride = layout.byteStride > 0 ? layout.byteStride : elemSize;
    layout.byteSize = layout.byteOffset + layout.numElements * stride;

    // Where byteSize is what the consumer bounds checks against -- and so has
    // to be at least the whole stream -- this is the exact span the stream
    // occupies, which is what may be READ out of Maya's buffer.
    //
    // The two differ on an interleaved buffer, where byteSize rounds the last
    // element up to a full stride and adds the offset on top, and so can
    // exceed Maya's real allocation. That costs the consumer nothing, because
    // it only ever compares. A copy out of Maya's buffer is not so forgiving:
    // reading past the end is an out-of-range GL copy that fails the whole
    // stream rather than clamping.
    layout.copyByteSize = layout.numElements > 0
        ? layout.byteOffset + (layout.numElements - 1) * stride + elemSize
        : 0;
    return layout;
}

static bool
_IsLayoutValid(const MhExtGpuSource& layout)
{
    return layout.elementType.type != HdTypeInvalid && layout.numElements > 0;
}

// The element type Hydra reads a shareable primvar as -- on the GPU, and as the CPU fallback /
// lazy readback -- or HdTypeInvalid for a primvar that is never shared. A VP2 stream is shared
// only when its element type is exactly this. The one place this mapping lives.
static HdType
_HydraCpuType(const TfToken& primvar)
{
    if (primvar == MayaHydraAdapterTokens->st) {
        return HdTypeFloatVec2;
    }
    if (primvar == UsdGeomTokens->points || primvar == UsdGeomTokens->normals
        || primvar == MayaHydraAdapterTokens->tangents) {
        return HdTypeFloatVec3;
    }
    return HdTypeInvalid;
}

// Call \p fn with a value of the C++ element type matching _HydraCpuType(primvar); the value
// only selects the type. Returns R() for a primvar that is never shared.
template <typename R, typename Fn>
static R
_WithHydraCpuElement(const TfToken& primvar, Fn&& fn)
{
    switch (_HydraCpuType(primvar)) {
    case HdTypeFloatVec2: return fn(GfVec2f());
    case HdTypeFloatVec3: return fn(GfVec3f());
    default: return R();
    }
}

// The stride to read a stream as T with, or 0 when its published layout
// cannot be read as T.
template <typename T>
static size_t
_ValidatedStride(uint64_t rawHandle, size_t byteStride, size_t tupleSize)
{
    const size_t elementSize = sizeof(T);
    if (tupleSize > 0 && elementSize != tupleSize) {
        TF_WARN(
            "Cannot read shared VP2 GL buffer %" PRIu64
            ": expected element size %zu does not match published layout tuple size %zu",
            rawHandle, elementSize, tupleSize);
        return 0;
    }
    const size_t stride = byteStride > 0 ? byteStride : elementSize;
    if (stride < elementSize) {
        TF_WARN(
            "Cannot read shared VP2 GL buffer %" PRIu64
            ": stride %zu is smaller than element size %zu",
            rawHandle, stride, elementSize);
        return 0;
    }
    return stride;
}

// Bytes a stream of \p numElements occupies from its first element: the
// last element is counted at its own size, not a full stride. Saturates
// instead of wrapping, so a bounds check against the result cannot be fooled
// by an overflowing layout.
template <typename T>
static size_t
_SpanBytes(size_t numElements, size_t stride)
{
    if (numElements == 0) {
        return 0;
    }
    if (stride != 0
        && numElements - 1 > (std::numeric_limits<size_t>::max() - sizeof(T)) / stride) {
        return std::numeric_limits<size_t>::max();
    }
    return (numElements - 1) * stride + sizeof(T);
}

// Copy \p count bytes into a destination of \p dstBytes, refusing to write
// past it. memcpy_s where the C library provides it (MSVC; glibc and Apple's
// libc do not), an explicit check otherwise.
static bool
_CopyBytes(void* dst, size_t dstBytes, const void* src, size_t count)
{
#if defined(_WIN32)
    return ::memcpy_s(dst, dstBytes, src, count) == 0;
#else
    if (count > dstBytes) {
        return false;
    }
    std::memcpy(dst, src, count);
    return true;
#endif
}

// De-interleave a stream's span -- its bytes from byteOffset on, in VP2's
// layout, \p spanBytes long -- into a VtArray. Pure CPU, so it runs on any
// thread. Returns an empty array when the layout would read past the span.
template <typename T>
static VtValue
_DecodeStream(
    const unsigned char* span,
    size_t               spanBytes,
    size_t               numElements,
    size_t               stride)
{
    const size_t required = _SpanBytes<T>(numElements, stride);
    if (required > spanBytes) {
        TF_WARN(
            "Cannot decode shared VP2 stream: %zu elements at stride %zu need "
            "%zu bytes, but only %zu were read",
            numElements, stride, required, spanBytes);
        return VtValue(VtArray<T>());
    }

    VtArray<T> result(numElements);
    const size_t resultBytes = numElements * sizeof(T);
    bool copied = true;
    if (stride == sizeof(T)) {
        copied = _CopyBytes(result.data(), resultBytes, span, resultBytes);
    } else {
        for (size_t i = 0; i < numElements && copied; ++i) {
            copied = _CopyBytes(
                &result[i], resultBytes - i * sizeof(T), span + i * stride, sizeof(T));
        }
    }
    if (!copied) {
        TF_WARN("Could not decode shared VP2 stream of %zu elements", numElements);
        return VtValue(VtArray<T>());
    }
    return VtValue(result);
}

// Synchronous readback of one stream through the shared readback context,
// under its global lock: one GPU round trip. Used for a stream's first pull,
// and for any pull the prefetch missed.
template <typename T>
static VtValue
_GetExtVertexBufferValue(
    uint64_t rawHandle,
    size_t   numElements,
    size_t   byteOffset,
    size_t   byteStride,
    size_t   tupleSize = 0)
{
    VtArray<T> result;
    if (rawHandle == 0 || numElements == 0) {
        return VtValue(result);
    }

    const size_t stride = _ValidatedStride<T>(rawHandle, byteStride, tupleSize);
    if (stride == 0) {
        return VtValue(result);
    }

    MhExtGpuBufferReadback::Lease resourceContext;
    if (!resourceContext) {
        return VtValue(result);
    }

    if (!glGetNamedBufferSubData) {
        TF_WARN(
            "Cannot read shared VP2 GL buffer %" PRIu64
            ": glGetNamedBufferSubData is unavailable",
            rawHandle);
        return VtValue(result);
    }

    // Do not use MVertexBuffer::map() here. Although it can happen to work
    // with some VP2 buffers, Maya limits direct vertex buffer access to
    // the main thread unless the buffer is software-staged or dual-memory.
    // This lazy value is pulled on Hydra workers, so read through the shared
    // GL context without entering Maya's main-thread-only buffer bookkeeping.
    VtValue value;
    if (stride == sizeof(T)) {
        result.resize(numElements);
        glGetNamedBufferSubData(
            static_cast<GLuint>(rawHandle),
            static_cast<GLintptr>(byteOffset),
            static_cast<GLsizeiptr>(numElements * sizeof(T)),
            result.data());
        value = VtValue(result);
    } else {
        std::vector<unsigned char> bytes(_SpanBytes<T>(numElements, stride));
        glGetNamedBufferSubData(
            static_cast<GLuint>(rawHandle),
            static_cast<GLintptr>(byteOffset),
            static_cast<GLsizeiptr>(bytes.size()),
            bytes.data());
        value = _DecodeStream<T>(bytes.data(), bytes.size(), numElements, stride);
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        TF_WARN(
            "Could not read shared VP2 GL buffer %" PRIu64
            " (OpenGL error 0x%x)",
            rawHandle, static_cast<unsigned int>(error));
        return VtValue(VtArray<T>());
    }
    return value;
}

// The readback key of a stream materialized as T. Prefetch and pull must
// build it identically, or every prefetched stream is read back twice.
// Templated on the stream so it can take the private stream type.
template <typename T, typename Stream>
static MhExtGpuBufferReadback::ReadKey
_ReadKeyFor(const Stream& stream)
{
    return { stream.rawHandle,
             stream.byteOffset,
             stream.byteStride,
             stream.numElements,
             std::type_index(typeid(T)) };
}

// Queue a stream's span for this frame's batched readback.
template <typename T, typename Stream>
static void
_PrefetchStream(const Stream& stream)
{
    if (stream.rawHandle == 0 || stream.numElements == 0) {
        return;
    }
    const size_t stride = stream.byteStride > 0 ? stream.byteStride : sizeof(T);
    if (stride < sizeof(T)) {
        return;
    }
    MhExtGpuBufferReadback::Prefetch(
        _ReadKeyFor<T>(stream), _SpanBytes<T>(stream.numElements, stride));
}

// The VP2 semantic of a stream, for log lines that need to say WHICH
// primvar was or was not shared.
//
// Mapped from the enum rather than taken from
// MVertexBufferDescriptor::semanticName(), which returns MString BY VALUE:
// asChar() on that temporary would dangle the moment this function
// returned. String literals have static storage, so these do not.
static const char* _ExtStreamName(MHWRender::MVertexBuffer* mvb)
{
    if (!mvb) {
        return "<none>";
    }
    switch (mvb->descriptor().semantic()) {
    case MHWRender::MGeometry::Semantic::kPosition: return "position";
    case MHWRender::MGeometry::Semantic::kNormal: return "normal";
    case MHWRender::MGeometry::Semantic::kTexture: return "texture";
    case MHWRender::MGeometry::Semantic::kColor: return "color";
    case MHWRender::MGeometry::Semantic::kTangent: return "tangent";
    case MHWRender::MGeometry::Semantic::kBitangent: return "bitangent";
    case MHWRender::MGeometry::Semantic::kTangentWithSign: return "tangentWithSign";
    case MHWRender::MGeometry::Semantic::kInvalidSemantic: return "<invalid>";
    }
    return "<unknown>";
}

// The VP2 semantic name _ExtStreamName reports for the stream feeding \p key, for log lines
// that have the primvar but no buffer.
static const char* _ExtStreamNameForKey(const TfToken& key)
{
    if (key == UsdGeomTokens->points)            { return "position"; }
    if (key == UsdGeomTokens->normals)           { return "normal"; }
    if (key == MayaHydraAdapterTokens->st)       { return "texture"; }
    if (key == MayaHydraAdapterTokens->tangents) { return "tangent"; }
    return "<unknown>";
}

uint8_t _LazyCpuBit(const TfToken& key)
{
    if (key == HdTokens->points) {
        return kLazyCpuPointsBit;
    }
    if (key == HdTokens->normals) {
        return kLazyCpuNormalsBit;
    }
    if (key == MayaHydraAdapterTokens->tangents) {
        return kLazyCpuTangentsBit;
    }
    if (key == MayaHydraAdapterTokens->st) {
        return kLazyCpuUvsBit;
    }
    return 0;
}

} // namespace

MhExtGpuSource MhExtGpuSource::FromDescriptor(
    const MHWRender::MVertexBufferDescriptor& descriptor,
    void*                                     resourceHandle,
    unsigned int                              vertexCount,
    const char*                               debugName)
{
    MhExtGpuSource source = _LayoutFromDescriptor(descriptor, vertexCount);
    if (!_IsLayoutValid(source) || !resourceHandle) {
        return {};
    }
    source.rawHandle = _ExtractRawHandle(resourceHandle);
    source.debugName = debugName ? debugName : "<none>";
    return source;
}

MhExtGpuStreams::MhExtGpuStreams(const MayaHydraAdapter& owner)
    : _owner(owner)
{
}

namespace {
std::atomic<bool> sRendererBindsDeviceStreams { true };
} // namespace

void MhExtGpuStreams::SetRendererBindsDeviceStreams(bool binds)
{
    sRendererBindsDeviceStreams.store(binds, std::memory_order_relaxed);
}

bool MhExtGpuStreams::RendererBindsDeviceStreams()
{
    return sRendererBindsDeviceStreams.load(std::memory_order_relaxed);
}

bool MhExtGpuStreams::IsEnabled()
{
    // Cached once at startup.
    static const bool enabled = TfGetEnvSetting(MAYAHYDRA_GPU_BUFFER_SHARING);
    return enabled;
}

void MhExtGpuStreams::BeginUpdate(bool geomChanged, bool topoChanged)
{
    // In hybrid mode a mesh starts batched (allowDirectBind=false) so static
    // meshes aggregate from frame one; the first real deform sticky-promotes it
    // to direct mode (zero per-frame copy). A deform is a second geometry-only
    // change since the mesh was last (re)populated (topology change) and
    // published a buffer. A display-mode toggle releases the old items' buffers
    // (geom+topo) and populates the new ones, after which VP2 sends one more
    // geometry-only change. Counting that as a deform would promote rigid meshes
    // for good, and direct ranges do not aggregate, so playback would then pay
    // per-prim draws.
    const _GpuBufferSharingMode sharingMode = _GetGpuBufferSharingMode();
    const bool                  hybridMode = sharingMode == _GpuBufferSharingMode::Hybrid;
    // Counted only while a hybrid mesh is still unclassified, and saturated at the
    // threshold, so it stays bounded for the life of the item.
    constexpr int kGeomOnlyUpdatesToPromote = 2;
    if (hybridMode && !_classifiedAsAnimating) {
        if (topoChanged) {
            _geomOnlyUpdatesSinceTopo = 0;
        } else if (geomChanged && _geomOnlyUpdatesSinceTopo < kGeomOnlyUpdatesToPromote) {
            ++_geomOnlyUpdatesSinceTopo;
        }
    }
    if (hybridMode && geomChanged && _geomOnlyUpdatesSinceTopo >= kGeomOnlyUpdatesToPromote
        && _positions && !_classifiedAsAnimating) {
        // No dirty emitted here. On the next stream read _Publish sees
        // that the cached allowDirectBind is now stale and republishes every
        // vertex-layer primvar, which dirties it. UV/tangent (not rewritten
        // during skeletal anim) are re-fetched the same way, so the new
        // direct-bind BAR keeps them.
        _classifiedAsAnimating = true;
    }
    // True when Storm holds a direct-bind BAR (direct mode, or hybrid after
    // promotion): the BAR keeps a stable reference to VP2's GL buffer, so a
    // byte-only deform is picked up at the next draw with no re-publish.
    _useDirectBind = sharingMode == _GpuBufferSharingMode::Direct
        || (hybridMode && _classifiedAsAnimating);

    // Reached on every geometry update of an adapter that shares streams
    // (UpdateFromDelta for GPU render items, the device-stream update for render
    // item override carriers), before any decision about individual streams.
    // Silence here therefore means geometry updates are not flowing for this
    // prim, rather than that sharing was declined.
    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] update: mode=%d directBind=%s geomChanged=%s topoChanged=%s\n",
        _owner.GetID().GetText(),
        int(sharingMode),
        _useDirectBind ? "yes" : "no",
        geomChanged ? "yes" : "no",
        topoChanged ? "yes" : "no");
}

bool MhExtGpuStreams::Share(
    MHWRender::MVertexBuffer* mvb,
    const TfToken&            key,
    bool                      alwaysDirty,
    bool&                     dirty)
{
    MH_PROFILE_FUNCTION();

    _Stream* stream = _StreamFor(key);
    if (!stream) {
        return false;
    }

    const char* const id = _owner.GetID().GetText();

    MhExtGpuBufferReadback::Capture();
    if (!mvb || mvb->vertexCount() == 0) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 gave empty vertex buffer\n",
            id, _ExtStreamName(mvb));
        return _ShareFailed(*stream, dirty);
    }
    void *resourceHandle = mvb->resourceHandle();
    if (!resourceHandle) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 gave no resource handle\n",
            id, _ExtStreamName(mvb));
        return _ShareFailed(*stream, dirty);
    }
    // resourceHandle() neither maps the buffer nor triggers a readback, so
    // identity can be checked every frame on every stream for almost nothing.
    const uint64_t rawHandle = _ExtractRawHandle(resourceHandle);
    if (rawHandle == 0) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: resource handle is null\n",
            id, _ExtStreamName(mvb));
        return _ShareFailed(*stream, dirty);
    }

    MhExtGpuSource source = _LayoutFromDescriptor(mvb->descriptor(), mvb->vertexCount());
    if (!_IsLayoutValid(source)) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 element type has no Hd equivalent "
            "(dataType=%d dimension=%d)\n",
            id, _ExtStreamName(mvb),
            int(mvb->descriptor().dataType()),
            mvb->descriptor().dimension());
        return _ShareFailed(*stream, dirty);
    }
    source.rawHandle = rawHandle;
    source.debugName = _ExtStreamName(mvb);

    const _PublishResult res = _Publish(source, *stream, key, _useDirectBind, false);
    if (res == _PublishResult::NoGpu) {
        return _ShareFailed(*stream, dirty);
    }
    _AfterPublish(res, *stream, key, alwaysDirty, dirty);
    return true;
}

bool MhExtGpuStreams::ShareDeviceOnly(
    const MhExtGpuSource& source,
    const TfToken&        key,
    bool                  alwaysDirty,
    bool&                 dirty)
{
    MH_PROFILE_FUNCTION();

    _Stream* stream = _StreamFor(key);
    if (!stream) {
        return false;
    }

    MhExtGpuBufferReadback::Capture();
    if (!source) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> NOTHING: the producer described no device buffer\n",
            _owner.GetID().GetText(), _ExtStreamNameForKey(key));
        return _ShareFailed(*stream, dirty);
    }

    const _PublishResult res = _Publish(source, *stream, key, _useDirectBind, true);
    if (res == _PublishResult::NoGpu) {
        return _ShareFailed(*stream, dirty);
    }
    _AfterPublish(res, *stream, key, alwaysDirty, dirty);
    return true;
}

bool MhExtGpuStreams::_ShareFailed(_Stream& stream, bool& dirty)
{
    // Withdraw a stream published earlier, so its schema does not keep naming a buffer VP2
    // may recycle. The caller reads the stream on the CPU instead.
    if (stream) {
        stream = {};
        dirty = true;
    }
    return false;
}

void MhExtGpuStreams::_AfterPublish(
    _PublishResult res,
    _Stream&       stream,
    const TfToken& key,
    bool           alwaysDirty,
    bool&          dirty)
{
    // In steady-state direct binding with a stable handle the stream stays clean
    // and Storm reads the new bytes straight from the aliased buffer. It dirties
    // on an identity/mode change (Republished), in batch mode (re-blit every
    // update), or once a consumer has pulled the lazy CPU fallback.
    //
    // A stream kept for readback only has no GPU consumer aliasing it, so every
    // update is new contents for whoever reads it.
    const bool lazyCpuConsumer = _LazyCpuBufferTriggered(key);
    if (res != _PublishResult::Unchanged || !_useDirectBind || alwaysDirty
        || lazyCpuConsumer) {
        dirty = true;
    }
    // A CPU consumer has pulled this stream before and, being dirtied above,
    // will pull it again this frame: queue its readback now, batched under
    // the producer frame's fence, instead of paying a GPU round trip per
    // stream at pull time. The element types match GetLazyValue.
    if (lazyCpuConsumer) {
        _WithHydraCpuElement<void>(key, [&stream](auto elementType) {
            _PrefetchStream<decltype(elementType)>(stream);
        });
    }
}

bool MhExtGpuStreams::Withdraw(const TfToken& key)
{
    _Stream* stream = _StreamFor(key);
    if (!stream || !*stream) {
        return false;
    }
    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s WITHDRAWN: gl=%" PRIu64 " is no longer in the "
        "render item's geometry\n",
        _owner.GetID().GetText(), _ExtStreamNameForKey(key), stream->rawHandle);
    *stream = {};
    return true;
}

size_t MhExtGpuStreams::SharedElementCount(const TfToken& key) const
{
    const _Stream* stream = _StreamFor(key);
    return (stream && *stream) ? stream->numElements : 0;
}

MhExtGpuStreams::_PublishResult
MhExtGpuStreams::_Publish(
    const MhExtGpuSource& source,
    _Stream&              stream,
    const TfToken&        key,
    bool                  allowDirectBind,
    bool                  deviceOnly)
{
    MH_PROFILE_FUNCTION();

    const char* const     id = _owner.GetID().GetText();
    const char* const     name = source.debugName;
    const uint64_t        rawHandle = source.rawHandle;
    const MhExtGpuSource& layout = source;

    // Shared only when Hydra can read the stream as-is: a consumer binds it with Hydra's
    // element type, and the lazy CPU readback decodes it as that type. Anything else (e.g.
    // 4-component tangents) is read on the CPU, which keeps only the components Hydra expects.
    const HdType hydraType = _HydraCpuType(key);
    if (layout.elementType.type != hydraType) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 element type %d does not match Hydra's %d for %s\n",
            id, name, int(layout.elementType.type),
            int(hydraType), key.GetText());
        return _PublishResult::NoGpu;
    }

    MayaHydraSceneIndex *sceneIndex = _owner.GetMayaHydraSceneIndex();
    const MhExtGpuBufferBridge bridge = sceneIndex
        ? MhExtGpuBufferBridge::ForHgi(sceneIndex->GetHgi())
        : MhExtGpuBufferBridge();
    if (!bridge) {
        // The negotiation failing, not an error: a VP2 that is not on GL, or
        // an Hgi with no arena to share through.
        const char* const hgiName = (sceneIndex && sceneIndex->GetHgi())
            ? sceneIndex->GetHgi()->GetAPIName().GetText()
            : "<none>";
        if (!deviceOnly) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s -> CPU: no bridge (renderer Hgi: %s)\n",
                id, name, hgiName);
            return _PublishResult::NoGpu;
        }

        // No CPU copy to fall back on, so keep the stream for the lazy
        // readback, which reads Maya's buffer through its own context and
        // needs no arena. No schema: there is nothing a renderer could bind.
        const bool wasReadbackOnly = stream.readbackOnly && stream.rawHandle == rawHandle
            && stream.numElements == layout.numElements;
        stream = {};
        stream.readbackOnly = true;
        stream.rawHandle = rawHandle;
        stream.numElements = layout.numElements;
        stream.byteOffset = layout.byteOffset;
        stream.byteStride = layout.byteStride;
        stream.tupleType = layout.elementType;
        if (!wasReadbackOnly) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s READBACK ONLY: gl=%" PRIu64 " elements=%zu, no bridge "
                "(renderer Hgi: %s)\n",
                id, name, rawHandle, layout.numElements, hgiName);
        }
        return _PublishResult::ReadbackOnly;
    }

    // Whether Maya's buffer identity is part of what was published, which is
    // the one place the two bridges genuinely disagree.
    //
    // A zero-copy bridge published Maya's buffer itself, so a new name is a
    // different allocation and has to be republished. A copying bridge
    // published its OWN allocation and only reads Maya's, so a new name
    // changes where the bytes come from and nothing about what the consumer
    // is bound to -- and VP2 hands out a new name routinely, recycling
    // buffers from frame to frame during playback. Treating that as a new
    // publication allocates a fresh exportable buffer per frame per object,
    // which exhausts the device's allocation count and takes the geometry
    // with it once the allocator starts refusing.
    const bool sourceIdentityMatters = bridge.IsZeroCopy();
    const bool layoutMatches = stream.numElements == layout.numElements
        && stream.byteOffset == layout.byteOffset
        && stream.byteStride == layout.byteStride
        && stream.tupleType == layout.elementType;

    // Same stream, same permission: the cached schema already describes it
    // exactly. A byte-only deform lands here -- VP2 rewrote the contents of a
    // buffer we published earlier.
    //
    // On a zero-copy bridge there is genuinely nothing to do: the consumer's
    // range still points at Maya's buffer and picks up the new bytes at the
    // next draw, which is what makes deforming geometry free on our side. A
    // copying bridge is looking at its own allocation instead, so the rewrite
    // has to be carried across -- but the layout is unchanged either way, so
    // this still republishes nothing.
    if (stream.schema
        && layoutMatches
        && stream.allowDirectBind == allowDirectBind
        && (!sourceIdentityMatters || stream.rawHandle == rawHandle)) {
        if (!bridge.IsZeroCopy()) {
            if (!bridge.Refresh(stream.buffer, static_cast<uint32_t>(rawHandle),
                                layout.copyByteSize)) {
                TF_DEBUG_GPU_BUFFER_SHARING(
                    "[%s] %s -> CPU: %s bridge could not refresh from "
                    "buffer %" PRIu64 "\n",
                    id, name,
                    bridge.Describe(), rawHandle);
                return _PublishResult::NoGpu;
            }
            // Only bookkeeping: which buffer the next refresh reads from.
            stream.rawHandle = rawHandle;
        }
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s unchanged: source buffer %" PRIu64 ", %zu elements "
            "(nothing republished)\n",
            id, name,
            rawHandle, layout.numElements);
        return _PublishResult::Unchanged;
    }

    // Reuse the existing buffer when what changed does not outgrow it -- the
    // direct-bind permission, or the source Maya reads from. Publishing one
    // stream twice would hand the consumer two buffer objects with no way to
    // tell they are the same memory, costing it the aggregation it keys on
    // buffer identity.
    HgiExternalBufferSharedPtr buffer = stream.buffer;
    const bool needsNewBuffer = !buffer
        || (sourceIdentityMatters
                ? (stream.rawHandle != rawHandle
                  || stream.numElements != layout.numElements
                  // The stride is recorded on the shared buffer's descriptor.
                  || stream.byteStride != layout.byteStride
                  || stream.tupleType != layout.elementType)
                // The copying bridge sized its allocation from the layout, so
                // any layout change outgrows it.
                : !layoutMatches);
    if (needsNewBuffer) {
        // A tightly packed stream publishes a zero byteStride; the buffer needs
        // the real stride, since HgiGL binds vertex input with it.
        const size_t vertexStride
            = layout.byteStride > 0 ? layout.byteStride : HdDataSizeOfTupleType(layout.elementType);
        buffer = bridge.Create(
            static_cast<uint32_t>(rawHandle),
            layout.byteSize,
            layout.copyByteSize,
            static_cast<uint32_t>(vertexStride),
            name);
        if (!buffer) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s -> CPU: %s bridge refused GL buffer %" PRIu64
                "\n",
                id, name,
                bridge.Describe(), rawHandle);
            return _PublishResult::NoGpu;
        }
    } else if (!bridge.IsZeroCopy()
                   && !bridge.Refresh(buffer, static_cast<uint32_t>(rawHandle),
                                      layout.copyByteSize)) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: %s bridge could not refresh from buffer "
            "%" PRIu64 "\n",
            id, name, bridge.Describe(),
            rawHandle);
        return _PublishResult::NoGpu;
    }

    // What goes into the scene index is a WEAK reference; the strong one is
    // kept below, in the stream (_Stream::buffer). A strong reference here would let any scene
    // index that caches or flattens this container pin GPU memory for the life
    // of the renderer.
    stream.schema =
        HdExtGpuBufferSchema::Builder()
            .SetExternalResource(
                HdRetainedTypedSampledDataSource<HdExternalBufferPtr>::New(
                    HdExternalBufferPtr(HgiExternalBufferWeakPtr(buffer))))
            .SetNumElements(
                HdRetainedTypedSampledDataSource<size_t>::New(
                    layout.numElements))
            .SetElementType(
                HdRetainedTypedSampledDataSource<HdTupleType>::New(
                    layout.elementType))
            .SetByteOffset(
                HdRetainedTypedSampledDataSource<size_t>::New(
                    layout.byteOffset))
            .SetByteStride(
                HdRetainedTypedSampledDataSource<size_t>::New(
                    layout.byteStride))
            .SetAllowDirectBind(
                HdRetainedTypedSampledDataSource<bool>::New(allowDirectBind))
            .Build();

    // Assigning drops our reference to whatever this stream published before,
    // which is safe even while the consumer is still drawing with it: the
    // consumer takes its own strong reference for as long as it might bind the
    // buffer, and the arena releases its own only from GarbageCollect(), once
    // the GPU work that named it has retired.
    stream.buffer = std::move(buffer);
    stream.rawHandle = rawHandle;
    stream.numElements = layout.numElements;
    stream.byteOffset = layout.byteOffset;
    stream.byteStride = layout.byteStride;
    stream.tupleType = layout.elementType;
    stream.allowDirectBind = allowDirectBind;
    stream.readbackOnly = false;

    // Logs the numbers the consumer bounds checks (byteOffset + numElements
    // * stride <= byteSize), because byteSize is derived here rather than
    // reported by Maya and getting it too small is the one failure that
    // silently disables sharing for an otherwise healthy stream.
    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s SHARED via %s%s: gl=%" PRIu64 " elements=%zu "
        "offset=%zu stride=%zu byteSize=%zu allowDirectBind=%s\n",
        id, name, bridge.Describe(),
        bridge.IsZeroCopy() ? "" : " (copied)", rawHandle,
        layout.numElements, layout.byteOffset, layout.byteStride,
        layout.byteSize, allowDirectBind ? "yes" : "no");

    return _PublishResult::Republished;
}

HdContainerDataSourceHandle
MhExtGpuStreams::GetSchema(const TfToken& key) const
{
    const _Stream* stream = _StreamFor(key);
    return stream ? stream->schema : nullptr;
}

MhExtGpuStreams::_Stream* MhExtGpuStreams::_StreamFor(const TfToken& key)
{
    return const_cast<_Stream*>(static_cast<const MhExtGpuStreams*>(this)->_StreamFor(key));
}

const MhExtGpuStreams::_Stream* MhExtGpuStreams::_StreamFor(const TfToken& key) const
{
    if (key == HdTokens->points)                 { return &_positions; }
    if (key == HdTokens->normals)                { return &_normals; }
    if (key == MayaHydraAdapterTokens->tangents) { return &_tangents; }
    if (key == MayaHydraAdapterTokens->st)       { return &_uvs; }
    return nullptr;
}

void MhExtGpuStreams::ResetLazyCpuConsumers()
{
    _lazyCpuBufferTriggeredMask.store(0, std::memory_order_relaxed);
}

bool MhExtGpuStreams::_LazyCpuBufferTriggered(const TfToken& key) const
{
    const uint8_t bit = _LazyCpuBit(key);
    return bit != 0
        && (_lazyCpuBufferTriggeredMask.load(std::memory_order_relaxed) & bit)
            != 0;
}

void MhExtGpuStreams::_SetLazyCpuBufferTriggered(const TfToken& key) const
{
    const uint8_t bit = _LazyCpuBit(key);
    if (bit != 0) {
        const uint8_t prev = _lazyCpuBufferTriggeredMask.fetch_or(
            bit, std::memory_order_relaxed);
        if ((prev & bit) == 0) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s: lazy CPU readback triggered for the FIRST time; "
                "primvar will now be dirtied on future geometry changes\n",
                _owner.GetID().GetText(), key.GetText());
        }
    }
}

VtValue MhExtGpuStreams::GetLazyValue(const TfToken& key) const
{
    MH_PROFILE_FUNCTION();

    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s: lazy CPU readback requested by consumer\n",
        _owner.GetID().GetText(), key.GetText());

    // The adapter is shared by all viewports. Once any consumer pulls this
    // CPU fallback, future changes to that primvar must be dirtied globally;
    // GPU-capable renderers still take extGpuBuffer and avoid this map.
    _SetLazyCpuBufferTriggered(key);

    // The element type argument only selects T; its value is unused.
    auto readStream = [](const _Stream& stream, auto elementType) {
        using T = decltype(elementType);
        const size_t tupleSize = stream.tupleType.type != HdTypeInvalid
            ? HdDataSizeOfTupleType(stream.tupleType)
            : 0;
        // Served from this frame's prefetch when Share queued one,
        // otherwise read back on demand. Either way, items drawing from the
        // same VP2 buffer read it once per frame.
        return MhExtGpuBufferReadback::GetOrRead(
            _ReadKeyFor<T>(stream),
            [&stream, tupleSize](const unsigned char* span, size_t spanBytes) {
                const size_t stride = _ValidatedStride<T>(
                    stream.rawHandle, stream.byteStride, tupleSize);
                return stride > 0
                    ? _DecodeStream<T>(span, spanBytes, stream.numElements, stride)
                    : VtValue(VtArray<T>());
            },
            [&stream, tupleSize]() {
                return _GetExtVertexBufferValue<T>(
                    stream.rawHandle,
                    stream.numElements,
                    stream.byteOffset,
                    stream.byteStride,
                    tupleSize);
            });
    };

    const _Stream* stream = _StreamFor(key);
    if (!stream) {
        return {};
    }
    return _WithHydraCpuElement<VtValue>(key, [&readStream, stream](auto elementType) {
        return readStream(*stream, elementType);
    });
}

#undef TF_DEBUG_GPU_BUFFER_SHARING

PXR_NAMESPACE_CLOSE_SCOPE
