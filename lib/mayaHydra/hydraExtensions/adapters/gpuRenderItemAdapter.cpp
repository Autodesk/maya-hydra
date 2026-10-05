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

#include <maya/MFnDagNode.h>
#include <maya/MViewport2Renderer.h>

#include <cinttypes> // PRIu64, for the GPU buffer sharing debug output
#include <cstring>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

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

// A resource context can be current on only one pull thread at a time. Holding
// this for the scope also protects one-time platform-context publication.
std::mutex _mayaResourceContextMutex;

#if defined(_WIN32)

// wglGetProcAddress and creation of a context in Maya's share group are done
// while Maya's render context is current. The context uses its own hidden
// window/DC, so a pull worker never competes for Maya's active drawable.
using _WglCreateContextAttribsArbProc =
    HGLRC(WINAPI *)(HDC, HGLRC, const int *);

struct _MayaWglContext
{
    HWND window = nullptr;
    HDC dc = nullptr;
    HGLRC context = nullptr;

    ~_MayaWglContext()
    {
        if (context) {
            wglDeleteContext(context);
        }
        if (dc && window) {
            ReleaseDC(window, dc);
        }
        if (window) {
            DestroyWindow(window);
        }
    }
};

_MayaWglContext _mayaWglContext;

void
_CaptureMayaWglContext()
{
    const HGLRC mayaContext = wglGetCurrentContext();
    const HDC mayaDc = wglGetCurrentDC();
    if (!mayaContext || !mayaDc) {
        return;
    }

    const auto createContextAttribs =
        reinterpret_cast<_WglCreateContextAttribsArbProc>(
            wglGetProcAddress("wglCreateContextAttribsARB"));
    if (!createContextAttribs) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mayaResourceContextMutex);
    if (_mayaWglContext.context) {
        return;
    }

    static constexpr wchar_t WindowClassName[] =
        L"MayaHydraSharedWglContextWindow";
    const HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSW windowClass {};
    windowClass.style = CS_OWNDC;
    windowClass.lpfnWndProc = DefWindowProcW;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = WindowClassName;
    if (!RegisterClassW(&windowClass)
        && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        TF_RUNTIME_ERROR(
            "Could not register the MayaHydra WGL window class "
            "(error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return;
    }

    const HWND window = CreateWindowExW(
        0,
        WindowClassName,
        L"",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        instance,
        nullptr);
    if (!window) {
        TF_RUNTIME_ERROR(
            "Could not create the MayaHydra WGL window (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return;
    }

    const HDC workerDc = GetDC(window);
    const int pixelFormat = GetPixelFormat(mayaDc);
    PIXELFORMATDESCRIPTOR pfd {};
    if (!workerDc
        || pixelFormat == 0
        || !DescribePixelFormat(
            mayaDc, pixelFormat, sizeof(pfd), &pfd)
        || !SetPixelFormat(workerDc, pixelFormat, &pfd)) {
        const DWORD error = GetLastError();
        if (workerDc) {
            ReleaseDC(window, workerDc);
        }
        DestroyWindow(window);
        TF_RUNTIME_ERROR(
            "Could not copy Maya's pixel format to the MayaHydra WGL "
            "window (error %lu)",
            static_cast<unsigned long>(error));
        return;
    }

    constexpr int WglContextMajorVersionArb = 0x2091;
    constexpr int WglContextMinorVersionArb = 0x2092;
    constexpr int WglContextProfileMaskArb = 0x9126;
    constexpr int WglContextCompatibilityProfileBitArb = 0x00000002;
    const int attribs[] = {
        WglContextMajorVersionArb, 4,
        WglContextMinorVersionArb, 5,
        WglContextProfileMaskArb,
        WglContextCompatibilityProfileBitArb,
        0
    };

    SetLastError(ERROR_SUCCESS);
    const HGLRC workerContext =
        createContextAttribs(workerDc, mayaContext, attribs);
    if (!workerContext) {
        const DWORD error = GetLastError();
        ReleaseDC(window, workerDc);
        DestroyWindow(window);
        TF_RUNTIME_ERROR(
            "Could not create a WGL context sharing Maya's resources "
            "through a hidden window (error %lu)",
            static_cast<unsigned long>(error));
        return;
    }

    _mayaWglContext.window = window;
    _mayaWglContext.dc = workerDc;
    _mayaWglContext.context = workerContext;
}

#elif defined(__linux__)

using _GlxCreateContextAttribsArbProc =
    GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

struct _MayaGlxContext
{
    Display* display = nullptr;
    GLXPbuffer pbuffer = 0;
    GLXContext context = nullptr;

    ~_MayaGlxContext()
    {
        if (context && display) {
            glXDestroyContext(display, context);
        }
        if (pbuffer && display) {
            glXDestroyPbuffer(display, pbuffer);
        }
    }
};

_MayaGlxContext _mayaGlxContext;

void
_CaptureMayaGlxContext()
{
    Display* const display = glXGetCurrentDisplay();
    const GLXContext mayaContext = glXGetCurrentContext();
    if (!display || !mayaContext) {
        return;
    }

    const auto createContextAttribs =
        reinterpret_cast<_GlxCreateContextAttribsArbProc>(
            glXGetProcAddressARB(
                reinterpret_cast<const GLubyte*>(
                    "glXCreateContextAttribsARB")));
    if (!createContextAttribs) {
        return;
    }

    std::lock_guard<std::mutex> lock(_mayaResourceContextMutex);
    if (_mayaGlxContext.context) {
        return;
    }

    int fbConfigId = 0;
    int screen = 0;
    if (glXQueryContext(
            display, mayaContext, GLX_FBCONFIG_ID, &fbConfigId) != Success
        || glXQueryContext(
            display, mayaContext, GLX_SCREEN, &screen) != Success) {
        TF_RUNTIME_ERROR(
            "Could not query Maya's GLX context configuration");
        return;
    }

    const int fbConfigAttribs[] = {
        GLX_FBCONFIG_ID, fbConfigId,
        None
    };
    int fbConfigCount = 0;
    GLXFBConfig* const fbConfigs =
        glXChooseFBConfig(
            display, screen, fbConfigAttribs, &fbConfigCount);
    if (!fbConfigs || fbConfigCount == 0) {
        if (fbConfigs) {
            XFree(fbConfigs);
        }
        TF_RUNTIME_ERROR(
            "Could not find Maya's GLX framebuffer configuration");
        return;
    }
    const GLXFBConfig fbConfig = fbConfigs[0];
    XFree(fbConfigs);

    const int pbufferAttribs[] = {
        GLX_PBUFFER_WIDTH, 1,
        GLX_PBUFFER_HEIGHT, 1,
        None
    };
    const GLXPbuffer pbuffer =
        glXCreatePbuffer(display, fbConfig, pbufferAttribs);
    if (!pbuffer) {
        TF_RUNTIME_ERROR(
            "Could not create the MayaHydra GLX pbuffer");
        return;
    }

    const int contextAttribs[] = {
        GLX_CONTEXT_MAJOR_VERSION_ARB, 4,
        GLX_CONTEXT_MINOR_VERSION_ARB, 5,
        GLX_CONTEXT_PROFILE_MASK_ARB,
        GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
        None
    };
    const GLXContext workerContext = createContextAttribs(
        display, fbConfig, mayaContext, True, contextAttribs);
    if (!workerContext) {
        glXDestroyPbuffer(display, pbuffer);
        TF_RUNTIME_ERROR(
            "Could not create a GLX context sharing Maya's resources");
        return;
    }

    _mayaGlxContext.display = display;
    _mayaGlxContext.pbuffer = pbuffer;
    _mayaGlxContext.context = workerContext;
}

#endif

// Lease the private drawable/context created in Maya's share group.
class MayaResourceContextScope
{
public:
#if defined(_WIN32)
    MayaResourceContextScope()
        : _contextLock(_mayaResourceContextMutex)
    {
        _dc = _mayaWglContext.dc;
        _context = _mayaWglContext.context;
        if (!_context || !_dc) {
            TF_CODING_ERROR(
                "Cannot read a shared VP2 vertex buffer before the "
                "MayaHydra shared WGL context has been created");
            return;
        }

        _previousContext = wglGetCurrentContext();
        _previousDc = wglGetCurrentDC();
        if (!wglMakeCurrent(_dc, _context)) {
            TF_RUNTIME_ERROR(
                "Could not make the hidden-window WGL context current "
                "(error %lu)",
                static_cast<unsigned long>(GetLastError()));
            return;
        }
        _isCurrent = true;
    }

    ~MayaResourceContextScope()
    {
        if (_isCurrent) {
            if (_previousContext) {
                TF_VERIFY(
                    wglMakeCurrent(_previousDc, _previousContext),
                    "Could not restore the worker's previous WGL context");
            } else {
                TF_VERIFY(
                    wglMakeCurrent(nullptr, nullptr),
                    "Could not release the shared WGL context");
            }
        }
    }
#elif defined(__linux__)
    MayaResourceContextScope()
        : _contextLock(_mayaResourceContextMutex)
    {
        _display = _mayaGlxContext.display;
        _pbuffer = _mayaGlxContext.pbuffer;
        _context = _mayaGlxContext.context;
        if (!_display || !_pbuffer || !_context) {
            TF_CODING_ERROR(
                "Cannot read a shared VP2 vertex buffer before the "
                "MayaHydra shared GLX context has been created");
            return;
        }

        _previousDisplay = glXGetCurrentDisplay();
        _previousDraw = glXGetCurrentDrawable();
        _previousRead = glXGetCurrentReadDrawable();
        _previousContext = glXGetCurrentContext();
        if (!glXMakeContextCurrent(
                _display, _pbuffer, _pbuffer, _context)) {
            TF_RUNTIME_ERROR(
                "Could not make the MayaHydra GLX pbuffer context current");
            return;
        }
        _isCurrent = true;
    }

    ~MayaResourceContextScope()
    {
        if (_isCurrent) {
            if (_previousContext && _previousDisplay) {
                TF_VERIFY(
                    glXMakeContextCurrent(
                        _previousDisplay,
                        _previousDraw,
                        _previousRead,
                        _previousContext),
                    "Could not restore the worker's previous GLX context");
            } else {
                TF_VERIFY(
                    glXMakeContextCurrent(
                        _display, None, None, nullptr),
                    "Could not release the MayaHydra GLX context");
            }
        }
    }
#else
    MayaResourceContextScope()
        : _contextLock(_mayaResourceContextMutex)
    {
        TF_CODING_ERROR(
            "Lazy external GPU-buffer readback is unsupported on this "
            "platform");
    }

    ~MayaResourceContextScope() = default;
#endif

    MayaResourceContextScope(const MayaResourceContextScope&) = delete;
    MayaResourceContextScope& operator=(
        const MayaResourceContextScope&) = delete;

    explicit operator bool() const { return _isCurrent; }

private:
    std::unique_lock<std::mutex> _contextLock;
#if defined(_WIN32)
    HDC                          _dc = nullptr;
    HGLRC                        _context = nullptr;
    HDC                          _previousDc = nullptr;
    HGLRC                        _previousContext = nullptr;
    bool                         _isCurrent = false;
#elif defined(__linux__)
    Display*                       _display = nullptr;
    GLXPbuffer                     _pbuffer = 0;
    GLXContext                     _context = nullptr;
    Display*                       _previousDisplay = nullptr;
    GLXDrawable                    _previousDraw = None;
    GLXDrawable                    _previousRead = None;
    GLXContext                     _previousContext = nullptr;
    bool                           _isCurrent = false;
#else
    bool _isCurrent = false;
#endif
};

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

    MayaResourceContextScope resourceContext;
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

    const size_t elementSize = sizeof(T);
    if (tupleSize > 0 && elementSize != tupleSize) {
        TF_WARN(
            "Cannot read shared VP2 GL buffer %" PRIu64
            ": expected element size %zu does not match published layout tuple size %zu",
            rawHandle, elementSize, tupleSize);
        return VtValue(result);
    }

    const size_t stride = byteStride > 0 ? byteStride : elementSize;
    if (stride < elementSize) {
        TF_WARN(
            "Cannot read shared VP2 GL buffer %" PRIu64
            ": stride %zu is smaller than element size %zu",
            rawHandle, stride, elementSize);
        return VtValue(result);
    }

    result.resize(numElements);
    // Do not use MVertexBuffer::map() here. Although it can happen to work
    // with some VP2 buffers, Maya limits direct OGSMayaVertexBuffer access to
    // the main thread unless the buffer is software-staged or dual-memory.
    // This lazy value is pulled on Hydra workers, so read through the shared
    // GL context without entering Maya's main-thread-only buffer bookkeeping.
    if (stride == elementSize) {
        glGetNamedBufferSubData(
            static_cast<GLuint>(rawHandle),
            static_cast<GLintptr>(byteOffset),
            static_cast<GLsizeiptr>(numElements * elementSize),
            result.data());
    } else {
        const size_t spanSize =
            (numElements - 1) * stride + elementSize;
        std::vector<unsigned char> bytes(spanSize);
        glGetNamedBufferSubData(
            static_cast<GLuint>(rawHandle),
            static_cast<GLintptr>(byteOffset),
            static_cast<GLsizeiptr>(spanSize),
            bytes.data());
        for (size_t i = 0; i < numElements; ++i) {
            std::memcpy(
                &result[i], bytes.data() + i * stride, elementSize);
        }
    }

    const GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        TF_WARN(
            "Could not read shared VP2 GL buffer %" PRIu64
            " (OpenGL error 0x%x)",
            rawHandle, static_cast<unsigned int>(error));
        result.clear();
    }
    return VtValue(result);
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

bool MayaHydraGpuRenderItemAdapter::_HasStoredPositions() const
{
    // GPU meshes keep no CPU positions; the published external buffer is the baseline.
    return MayaHydraRenderItemAdapter::_HasStoredPositions() || _extPositions;
}

size_t MayaHydraGpuRenderItemAdapter::_StoredPositionCount() const
{
    return _extPositions ? _extPositions.numElements
                         : MayaHydraRenderItemAdapter::_StoredPositionCount();
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
    if (res == _ExtPublishResult::Republished || !_useDirectBind || alwaysDirty
        || _LazyCpuBufferTriggered(key)) {
        dirty = true;
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
    switch (mvb->descriptor().semantic()) {
    case MGeometry::Semantic::kPosition:
        // Points also drive Hydra's smooth-normals recompute when Maya does not
        // supply normals, so they dirty whenever Hydra owns normals -- even under
        // stable direct binding.
        if (_ShareStream(mvb, _extPositions, HdTokens->points, !useMayaNormals, dirty.positions)) {
            _positions.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kNormal:
        if (useMayaNormals
            && _ShareStream(mvb, _extNormals, HdTokens->normals, false, dirty.normals)) {
            _normals.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTexture:
        if (isMesh && _ShareStream(mvb, _extUvs, MayaHydraAdapterTokens->st, false, dirty.uvs)) {
            _uvs.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTangent:
        if (isMesh
            && _ShareStream(
                mvb, _extTangents, MayaHydraAdapterTokens->tangents, false, dirty.tangents)) {
            _tangents.clear();
            return;
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
        bool hasPositions = false;
        bool hasNormals = false;
        bool hasUvs = false;
        bool hasTangents = false;
        for (int vbIdx = 0; vbIdx < vertexBufferCount; vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if (!mvb) {
                continue;
            }
            switch (mvb->descriptor().semantic()) {
            case MGeometry::Semantic::kPosition: hasPositions = true; break;
            case MGeometry::Semantic::kNormal: hasNormals = true; break;
            case MGeometry::Semantic::kTexture: hasUvs = true; break;
            case MGeometry::Semantic::kTangent: hasTangents = true; break;
            default: break;
            }
        }
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
        withdraw(_extPositions, hasPositions, dirty.positions, "position");
        withdraw(_extNormals, hasNormals, dirty.normals, "normal");
        withdraw(_extUvs, hasUvs, dirty.uvs, "texture");
        withdraw(_extTangents, hasTangents, dirty.tangents, "tangent");
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

#if defined(_WIN32)
    _CaptureMayaWglContext();
#elif defined(__linux__)
    _CaptureMayaGlxContext();
#endif
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
        return _GetExtVertexBufferValue<T>(
            stream.rawHandle,
            stream.numElements,
            stream.byteOffset,
            stream.byteStride,
            tupleSize);
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
