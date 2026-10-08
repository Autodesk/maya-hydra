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

#include "gpuRenderItemAdapter.h"
#include "mhExtGpuBufferReadback.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mhExtGpuBufferBridge.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/profilingUtils.h>

#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/hd/extGpuBufferSchema.h>
#include <pxr/imaging/hd/externalBuffer.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/imaging/hgi/enums.h>
#include <pxr/imaging/hgi/externalBuffer.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <maya/MFnDagNode.h>
#include <maya/MViewport2Renderer.h>

#include <cinttypes> // PRIu64, for the GPU buffer sharing debug output
#include <cstring> // memcpy; memcpy_s on MSVC
#include <limits>
#include <string>
#include <typeindex>
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

// Cached once at startup.
bool _IsGpuBufferSharingEnabled()
{
    static const bool enabled = TfGetEnvSetting(MAYAHYDRA_GPU_BUFFER_SHARING);
    return enabled;
}

// Object-space AABB of a render item's source shape, taken from the DAG node.
// MRenderItem::boundingBox() is frequently empty while a scene is still loading,
// and Hydra render items publish vertex data as GPU-only buffers (no CPU
// points), so neither the render-item bbox nor the vertex buffers are a
// reliable bounds source. VP2 itself derives bounds from the source shape's
// MFnDagNode::boundingBox(), which Maya maintains in object space regardless of
// where the vertex data lives; we use the same source. Returns an empty range
// if the DAG node has no usable bounding box.
GfRange3d _ObjectSpaceBoundsFromDagNode(const MDagPath& dagPath)
{
    GfRange3d range;
    MStatus   status;
    MFnDagNode dagNode(dagPath, &status);
    if (!status) {
        return range;
    }
    const MBoundingBox bb = dagNode.boundingBox(&status);
    if (!status) {
        return range;
    }
    const MPoint mn = bb.min();
    const MPoint mx = bb.max();
    // Reject an empty/inverted box; a flat shape (zero extent on one axis) is
    // still valid, so compare per-axis with min <= max.
    if (mn.x <= mx.x && mn.y <= mx.y && mn.z <= mx.z) {
        range = GfRange3d(GfVec3d(mn.x, mn.y, mn.z), GfVec3d(mx.x, mx.y, mx.z));
    }
    return range;
}

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
// allocation size that provably contains it.
struct _ExtLayout
{
    HdTupleType elementType = { HdTypeInvalid, 1 };
    size_t numElements = 0;
    size_t byteOffset = 0;
    size_t byteStride = 0;
    size_t byteSize = 0;
    size_t copyByteSize = 0;

    bool IsValid() const
    {
        return elementType.type != HdTypeInvalid && numElements > 0;
    }
};

static _ExtLayout
_GetExtLayout(MHWRender::MVertexBuffer *mvb)
{
    _ExtLayout layout;
    const MHWRender::MVertexBufferDescriptor &desc = mvb->descriptor();
    layout.elementType = _ToHdTupleType(desc.dataType(), desc.dimension());
    if (layout.elementType.type == HdTypeInvalid) {
        return layout;
    }
    layout.numElements = mvb->vertexCount();

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
    layout.copyByteSize =
        layout.byteOffset + (layout.numElements - 1) * stride + elemSize;
    return layout;
}

// Byte width Hydra expects for the CPU fallback / lazy readback of a primvar.
static size_t
_HydraCpuElementSize(const TfToken& primvar)
{
    if (primvar == MayaHydraAdapterTokens->st) {
        return sizeof(GfVec2f);
    }
    if (primvar == UsdGeomTokens->points || primvar == UsdGeomTokens->normals
        || primvar == MayaHydraAdapterTokens->tangents) {
        return sizeof(GfVec3f);
    }
    return 0;
}

static bool
_ExtLayoutMatchesHydraCpu(const _ExtLayout& layout, const TfToken& primvar)
{
    const size_t expected = _HydraCpuElementSize(primvar);
    return layout.IsValid() && expected != 0
        && HdDataSizeOfTupleType(layout.elementType) == expected;
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
    // with some VP2 buffers, Maya limits direct OGSMayaVertexBuffer access to
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
// Templated on the stream so it can take the adapter's private stream type.
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
    case MGeometry::Semantic::kPosition: return "position";
    case MGeometry::Semantic::kNormal: return "normal";
    case MGeometry::Semantic::kTexture: return "texture";
    case MGeometry::Semantic::kColor: return "color";
    case MGeometry::Semantic::kTangent: return "tangent";
    case MGeometry::Semantic::kBitangent: return "bitangent";
    case MGeometry::Semantic::kTangentWithSign: return "tangentWithSign";
    case MGeometry::Semantic::kInvalidSemantic: return "<invalid>";
    }
    return "<unknown>";
}

} // namespace

MayaHydraGpuRenderItemAdapter::MayaHydraGpuRenderItemAdapter(
    const MDagPath&      dagPath,
    const SdfPath&       slowId,
    int                  fastId,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MRenderItem&   ri,
    TfToken              purposeRenderTag)
    : MayaHydraRenderItemAdapter(
        dagPath, slowId, fastId, mayaHydraSceneIndex, ri, purposeRenderTag)
{
}

bool MayaHydraGpuRenderItemAdapter::IsEligible(const MRenderItem& ri, Hgi* hgi)
{
    if (!_IsGpuBufferSharingEnabled()) {
        return false;
    }
    // Triangles/strips -> HdMesh, lines/line-strips -> HdBasisCurves. Points
    // are intentionally excluded: point render items are not populated by
    // UpdateFromDelta yet (see its early-exit guard), so there is nothing to
    // share.
    switch (ri.primitive()) {
    case MGeometry::Primitive::kTriangles:
    case MGeometry::Primitive::kTriangleStrip:
    case MGeometry::Primitive::kLines:
    case MGeometry::Primitive::kLineStrip: break;
    default:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] Render item -> CPU: primitive type %d is not eligible for GPU buffer sharing\n",
            ri.name().asChar(), int(ri.primitive()));
        return false;
    }
    // A VP2 that is not on GL, or a renderer Hgi with no arena to share through (e.g. Metal),
    // can never share a stream, so such items stay on the CPU path entirely.
    const bool bridgeAvailable = static_cast<bool>(MhExtGpuBufferBridge::ForHgi(hgi));
    if (!bridgeAvailable) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] Render item -> CPU: renderer Hgi (%s) has no GPU buffer sharing bridge\n",
            ri.name().asChar(),
            hgi ? hgi->GetAPIName().GetText() : "<null>");
        return false;
    }
    return true;
}

size_t MayaHydraGpuRenderItemAdapter::_StoredStreamCount(const TfToken& primvar) const
{
    // A shared stream's CPU array is cleared once it is published, so the published
    // external buffer is the baseline.
    const _ExtStream* stream = nullptr;
    if (primvar == UsdGeomTokens->points) {
        stream = &_extPositions;
    } else if (primvar == UsdGeomTokens->normals) {
        stream = &_extNormals;
    } else if (primvar == MayaHydraAdapterTokens->st) {
        stream = &_extUvs;
    } else if (primvar == MayaHydraAdapterTokens->tangents) {
        stream = &_extTangents;
    }
    if (stream && *stream) {
        const size_t expected = _HydraCpuElementSize(primvar);
        const size_t tupleSize = stream->tupleType.type != HdTypeInvalid
            ? HdDataSizeOfTupleType(stream->tupleType)
            : 0;
        if (expected != 0 && tupleSize == expected) {
            return stream->numElements;
        }
    }
    return MayaHydraRenderItemAdapter::_StoredStreamCount(primvar);
}

void MayaHydraGpuRenderItemAdapter::_BeginGeometryUpdate(bool geomChanged, bool topoChanged)
{
    // In hybrid mode a mesh starts batched (allowDirectBind=false) so static
    // meshes aggregate from frame one; the first real deform (geom/topo change
    // on a mesh that already published a buffer -- filtering the initial
    // populate frame) sticky-promotes it to direct mode (zero per-frame copy).
    const _GpuBufferSharingMode sharingMode = _GetGpuBufferSharingMode();
    const bool                  hybridMode = sharingMode == _GpuBufferSharingMode::Hybrid;
    if (hybridMode && (geomChanged || topoChanged) && _extPositions && !_classifiedAsAnimating) {
        // No dirty emitted here. On the next stream read _PublishExtStream sees
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

    // Reached for every GPU render item on every update, before any decision
    // about individual streams. Silence here therefore means UpdateFromDelta is
    // not running for this prim at all -- render items are not flowing --
    // rather than that sharing was declined.
    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] update: mode=%d directBind=%s geomChanged=%s topoChanged=%s\n",
        GetID().GetText(),
        int(sharingMode),
        _useDirectBind ? "yes" : "no",
        geomChanged ? "yes" : "no",
        topoChanged ? "yes" : "no");
}

GfRange3d MayaHydraGpuRenderItemAdapter::_ResolveBounds(const GfRange3d& renderItemBounds) const
{
    // GPU-buffer render items often have an empty MRenderItem bbox (vertex
    // data is GPU-only); fall back to the source shape's DAG-node bounds so
    // the prim isn't culled.
    return renderItemBounds.IsEmpty() ? _ObjectSpaceBoundsFromDagNode(GetDagPath())
                                      : renderItemBounds;
}

bool MayaHydraGpuRenderItemAdapter::_ShareStream(
    MVertexBuffer* mvb,
    _ExtStream&    stream,
    const TfToken& key,
    bool           alwaysDirty,
    bool&          dirty)
{
    MH_PROFILE_FUNCTION();

    const _ExtPublishResult res = _PublishExtStream(mvb, stream, _useDirectBind);
    if (res == _ExtPublishResult::NoGpu) {
        stream = {};
        return false;
    }
    // In steady-state direct binding with a stable handle the stream stays clean
    // and Storm reads the new bytes straight from the aliased buffer. It dirties
    // on an identity/mode change (Republished), in batch mode (re-blit every
    // update), or once a consumer has pulled the lazy CPU fallback.
    const bool lazyCpuConsumer = _LazyCpuBufferTriggered(key);
    if (res == _ExtPublishResult::Republished || !_useDirectBind || alwaysDirty
        || lazyCpuConsumer) {
        dirty = true;
    }
    // A CPU consumer has pulled this stream before and, being dirtied above,
    // will pull it again this frame: queue its readback now, batched under
    // the producer frame's fence, instead of paying a GPU round trip per
    // stream at pull time. The element types match GetExtGpuBufferLazyValue.
    if (lazyCpuConsumer) {
        if (key == MayaHydraAdapterTokens->st) {
            _PrefetchStream<GfVec2f>(stream);
        } else {
            _PrefetchStream<GfVec3f>(stream);
        }
    }
    return true;
}

void MayaHydraGpuRenderItemAdapter::_ReadVertexStream(
    MVertexBuffer* mvb,
    bool           topoChanged,
    bool           useMayaNormals,
    _StreamDirty&  dirty)
{
    MH_PROFILE_FUNCTION();

    const bool isMesh = GetPrimitive() == MGeometry::Primitive::kTriangles
        || GetPrimitive() == MGeometry::Primitive::kTriangleStrip;
    const _ExtLayout extLayout = _GetExtLayout(mvb);
    const auto       canShare = [&extLayout](const TfToken& primvar) {
        return _ExtLayoutMatchesHydraCpu(extLayout, primvar);
    };
    switch (mvb->descriptor().semantic()) {
    case MGeometry::Semantic::kPosition:
        // Points also drive Hydra's smooth-normals recompute when Maya does not
        // supply normals, so they dirty whenever Hydra owns normals -- even under
        // stable direct binding.
        if (canShare(HdTokens->points)
            && _ShareStream(mvb, _extPositions, HdTokens->points, !useMayaNormals, dirty.positions)) {
            _positions.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kNormal:
        if (useMayaNormals && canShare(HdTokens->normals)
            && _ShareStream(mvb, _extNormals, HdTokens->normals, false, dirty.normals)) {
            _normals.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTexture:
        if (isMesh && canShare(MayaHydraAdapterTokens->st)
            && _ShareStream(mvb, _extUvs, MayaHydraAdapterTokens->st, false, dirty.uvs)) {
            _uvs.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTangent:
        if (isMesh && canShare(MayaHydraAdapterTokens->tangents)) {
            if (_ShareStream(
                    mvb, _extTangents, MayaHydraAdapterTokens->tangents, false, dirty.tangents)) {
                _tangents.clear();
                return;
            }
        } else {
            _extTangents = {};
        }
        if (isMesh && extLayout.IsValid() && !canShare(MayaHydraAdapterTokens->tangents)) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s -> CPU: VP2 tuple size %zu does not match Hydra %zu-byte "
                "tangents; using map() projection\n",
                GetID().GetText(),
                _ExtStreamName(mvb),
                HdDataSizeOfTupleType(extLayout.elementType),
                _HydraCpuElementSize(MayaHydraAdapterTokens->tangents));
        }
        break;
    case MGeometry::Semantic::kColor:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: vertex colors not supported on GPU path yet\n",
            GetID().GetText(), _ExtStreamName(mvb));
        break;
    default:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: semantic %d not shared\n",
            GetID().GetText(), _ExtStreamName(mvb), int(mvb->descriptor().semantic()));
        break;
    }

    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s falling back to CPU MVertexBuffer::map() read\n",
        GetID().GetText(), _ExtStreamName(mvb));

    MayaHydraRenderItemAdapter::_ReadVertexStream(mvb, topoChanged, useMayaNormals, dirty);
}

void MayaHydraGpuRenderItemAdapter::_EndGeometryUpdate(
    MGeometry*    geom,
    int           vertexBufferCount,
    bool          geomChanged,
    bool          topoChanged,
    bool          useMayaNormals,
    _StreamDirty& dirty)
{
    // A stream published earlier but absent from this geometry must be
    // withdrawn. VP2 can release a hidden item's buffers (a display-mode
    // switch does) and recycle the names, and the stream reads only visit the
    // buffers that are still there -- so without this the cached schema keeps
    // naming a GL buffer VP2 no longer guarantees, and Storm copies from or
    // draws through whatever that name now is.
    if (geomChanged || topoChanged) {
        const _StreamPresence present = _GetStreamPresence(geom, vertexBufferCount);
        auto withdraw = [this](_ExtStream& stream, bool present, bool& streamDirty,
                               const char* name) {
            if (!stream || present) {
                return;
            }
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s WITHDRAWN: gl=%" PRIu64 " is no longer in the "
                "render item's geometry\n",
                GetID().GetText(), name, stream.rawHandle);
            stream = {};
            streamDirty = true;
        };
        withdraw(_extPositions, present.positions, dirty.positions, "position");
        withdraw(_extNormals, present.normals, dirty.normals, "normal");
        withdraw(_extUvs, present.uvs, dirty.uvs, "texture");
        withdraw(_extTangents, present.tangents, dirty.tangents, "tangent");
    }

    // Streams read on this update are already dirty. A stream VP2 did not supply has nothing
    // to re-pull on a deformation-only frame (playback sets geomChanged without topoChanged); it
    // can only have appeared or changed when Maya flags a topology change.
    if (geomChanged && topoChanged) {
        dirty.positions = dirty.positions || !_extPositions;
        dirty.uvs = dirty.uvs || !_extUvs;
        dirty.tangents = dirty.tangents || !_extTangents;
        dirty.normals = dirty.normals || (useMayaNormals && !_extNormals);
    }
}

bool MayaHydraGpuRenderItemAdapter::_TopologyNeedsRebuild(
    bool emitTopologyLocators,
    bool hasCachedTopology) const
{
    // Only (re)build the cached topology when connectivity actually changed
    // (== emitTopologyLocators, which already ran the connectivity diff) or when there is no
    // cached topology yet. Maya raises MVS_changedGeometry every frame during deformation with
    // unchanged connectivity; rebuilding on those frames would rescan the whole face-vertex index
    // array (HdMeshTopology::ComputeNumPoints) and reallocate for nothing - no topology-dirty
    // locator is emitted on those frames, so Storm never re-pulls the rebuilt copy.
    return emitTopologyLocators || !hasCachedTopology;
}

MayaHydraGpuRenderItemAdapter::_ExtPublishResult
MayaHydraGpuRenderItemAdapter::_PublishExtStream(
    MHWRender::MVertexBuffer *mvb,
    _ExtStream               &stream,
    bool                      allowDirectBind)
{
    MH_PROFILE_FUNCTION();

    MhExtGpuBufferReadback::Capture();
    if (!mvb || mvb->vertexCount() == 0) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 gave empty vertex buffer\n",
            GetID().GetText(), _ExtStreamName(mvb));
        return _ExtPublishResult::NoGpu;
    }
    void *resourceHandle = mvb->resourceHandle();
    if (!resourceHandle) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 gave no resource handle\n",
            GetID().GetText(), _ExtStreamName(mvb));
        return _ExtPublishResult::NoGpu;
    }
    // resourceHandle() neither maps the buffer nor triggers a readback, so
    // identity can be checked every frame on every stream for almost nothing.
    const uint64_t rawHandle = _ExtractRawHandle(resourceHandle);
    if (rawHandle == 0) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: resource handle is null\n",
            GetID().GetText(), _ExtStreamName(mvb));
        return _ExtPublishResult::NoGpu;
    }

    const _ExtLayout layout = _GetExtLayout(mvb);
    if (!layout.IsValid()) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: VP2 element type has no Hd equivalent "
            "(dataType=%d dimension=%d)\n",
            GetID().GetText(), _ExtStreamName(mvb),
            int(mvb->descriptor().dataType()),
            mvb->descriptor().dimension());
        return _ExtPublishResult::NoGpu;
    }

    MayaHydraSceneIndex *sceneIndex = GetMayaHydraSceneIndex();
    const MhExtGpuBufferBridge bridge = sceneIndex
        ? MhExtGpuBufferBridge::ForHgi(sceneIndex->GetHgi())
        : MhExtGpuBufferBridge();
    if (!bridge) {
        // The negotiation failing, not an error: a VP2 that is not on GL, or
        // an Hgi with no arena to share through.
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: no bridge (renderer Hgi: %s)\n",
            GetID().GetText(), _ExtStreamName(mvb),
            (sceneIndex && sceneIndex->GetHgi())
                ? sceneIndex->GetHgi()->GetAPIName().GetText()
                : "<none>");
        return _ExtPublishResult::NoGpu;
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
                    GetID().GetText(), _ExtStreamName(mvb),
                    bridge.Describe(), rawHandle);
                return _ExtPublishResult::NoGpu;
            }
            // Only bookkeeping: which buffer the next refresh reads from.
            stream.rawHandle = rawHandle;
        }
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s unchanged: source buffer %" PRIu64 ", %zu elements "
            "(nothing republished)\n",
            GetID().GetText(), _ExtStreamName(mvb),
            rawHandle, layout.numElements);
        return _ExtPublishResult::Unchanged;
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
                   || stream.numElements != layout.numElements)
                // The copying bridge sized its allocation from the layout, so
                // any layout change outgrows it.
                : !layoutMatches);
    if (needsNewBuffer) {
        buffer = bridge.Create(
            static_cast<uint32_t>(rawHandle),
            layout.byteSize,
            layout.copyByteSize,
            _ExtStreamName(mvb));
        if (!buffer) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s -> CPU: %s bridge refused GL buffer %" PRIu64
                "\n",
                GetID().GetText(), _ExtStreamName(mvb),
                bridge.Describe(), rawHandle);
            return _ExtPublishResult::NoGpu;
        }
    } else if (!bridge.IsZeroCopy()
                   && !bridge.Refresh(buffer, static_cast<uint32_t>(rawHandle),
                                      layout.copyByteSize)) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: %s bridge could not refresh from buffer "
            "%" PRIu64 "\n",
            GetID().GetText(), _ExtStreamName(mvb), bridge.Describe(),
            rawHandle);
        return _ExtPublishResult::NoGpu;
    }

    // What goes into the scene index is a WEAK reference; the strong one is
    // kept below, in the adapter. A strong reference here would let any scene
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

    // Logs the numbers the consumer bounds checks (byteOffset + numElements
    // * stride <= byteSize), because byteSize is derived here rather than
    // reported by Maya and getting it too small is the one failure that
    // silently disables sharing for an otherwise healthy stream.
    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s SHARED via %s%s: gl=%" PRIu64 " elements=%zu "
        "offset=%zu stride=%zu byteSize=%zu allowDirectBind=%s\n",
        GetID().GetText(), _ExtStreamName(mvb), bridge.Describe(),
        bridge.IsZeroCopy() ? "" : " (copied)", rawHandle,
        layout.numElements, layout.byteOffset, layout.byteStride,
        layout.byteSize, allowDirectBind ? "yes" : "no");

    return _ExtPublishResult::Republished;
}

HdContainerDataSourceHandle
MayaHydraGpuRenderItemAdapter::GetExtGpuBufferSchema(const TfToken& key) const
{
    if (key == HdTokens->points)                 { return _extPositions.schema; }
    if (key == HdTokens->normals)                { return _extNormals.schema; }
    if (key == MayaHydraAdapterTokens->tangents) { return _extTangents.schema; }
    if (key == MayaHydraAdapterTokens->st)       { return _extUvs.schema; }
    return nullptr;
}

bool
MayaHydraGpuRenderItemAdapter::_LazyCpuBufferTriggered(const TfToken& key) const
{
    uint8_t bit = 0;
    if (key == HdTokens->points) {
        bit = kLazyCpuPointsBit;
    } else if (key == HdTokens->normals) {
        bit = kLazyCpuNormalsBit;
    } else if (key == MayaHydraAdapterTokens->tangents) {
        bit = kLazyCpuTangentsBit;
    } else if (key == MayaHydraAdapterTokens->st) {
        bit = kLazyCpuUvsBit;
    }
    return bit != 0
        && (_lazyCpuBufferTriggeredMask.load(std::memory_order_relaxed) & bit)
            != 0;
}

void
MayaHydraGpuRenderItemAdapter::_SetLazyCpuBufferTriggered(
    const TfToken& key) const
{
    uint8_t bit = 0;
    if (key == HdTokens->points) {
        bit = kLazyCpuPointsBit;
    } else if (key == HdTokens->normals) {
        bit = kLazyCpuNormalsBit;
    } else if (key == MayaHydraAdapterTokens->tangents) {
        bit = kLazyCpuTangentsBit;
    } else if (key == MayaHydraAdapterTokens->st) {
        bit = kLazyCpuUvsBit;
    }
    if (bit != 0) {
        const uint8_t prev = _lazyCpuBufferTriggeredMask.fetch_or(
            bit, std::memory_order_relaxed);
        if ((prev & bit) == 0) {
            TF_DEBUG_GPU_BUFFER_SHARING(
                "[%s] %s: lazy CPU readback triggered for the FIRST time; "
                "primvar will now be dirtied on future geometry changes\n",
                GetID().GetText(), key.GetText());
        }
    }
}

VtValue
MayaHydraGpuRenderItemAdapter::GetExtGpuBufferLazyValue(const TfToken& key) const
{
    MH_PROFILE_FUNCTION();

    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s: lazy CPU readback requested by consumer\n",
        GetID().GetText(), key.GetText());

    // The adapter is shared by all viewports. Once any consumer pulls this
    // CPU fallback, future changes to that primvar must be dirtied globally;
    // GPU-capable renderers still take extGpuBuffer and avoid this map.
    _SetLazyCpuBufferTriggered(key);

    // The element type argument only selects T; its value is unused.
    auto readStream = [](const _ExtStream& stream, auto elementType) {
        using T = decltype(elementType);
        const size_t tupleSize = stream.tupleType.type != HdTypeInvalid
            ? HdDataSizeOfTupleType(stream.tupleType)
            : 0;
        // Served from this frame's prefetch when _ShareStream queued one,
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

    if (key == HdTokens->points) {
        return readStream(_extPositions, GfVec3f());
    }
    if (key == HdTokens->normals) {
        return readStream(_extNormals, GfVec3f());
    }
    if (key == MayaHydraAdapterTokens->tangents) {
        return readStream(_extTangents, GfVec3f());
    }
    if (key == MayaHydraAdapterTokens->st) {
        return readStream(_extUvs, GfVec2f());
    }
    return {};
}

///////////////////////////////////////////////////////////////////////
// TF_REGISTRY
///////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraGpuRenderItemAdapter, TfType::Bases<MayaHydraRenderItemAdapter>>();
}

#undef TF_DEBUG_GPU_BUFFER_SHARING

PXR_NAMESPACE_CLOSE_SCOPE
