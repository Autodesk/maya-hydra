//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#include "renderItemAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/adapters/renderItemTopologyUtil.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/plug/plugin.h>
#include <pxr/base/plug/registry.h>
#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/usd/sdr/registry.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/usdImaging/usdImaging/tokens.h>
#if defined(USD_HAS_GPU_BUFFER_SHARING)
#include <pxr/base/tf/envSetting.h>
#include <pxr/imaging/hd/extGpuBufferSchema.h>
#include <pxr/imaging/hd/externalBuffer.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/imaging/hgi/enums.h>
#include <pxr/imaging/hgi/externalBuffer.h>
#include <pxr/imaging/hgi/hgi.h>
#include <mayaHydraLib/adapters/mhExtGpuBufferBridge.h>
#include <pxr/imaging/hgi/tokens.h>
#endif

#include <maya/MAnimControl.h>
#include <maya/MDGContextGuard.h>
#include <maya/MFn.h>
#include <maya/MNodeMessage.h>
#if defined(USD_HAS_GPU_BUFFER_SHARING)
#include <maya/MFnDagNode.h>
#include <maya/MViewport2Renderer.h>
#endif

#include <functional>
#if defined(USD_HAS_GPU_BUFFER_SHARING)
#include <cinttypes>  // PRIu64, for the GPU buffer sharing debug output
#include <cstring>
#include <mutex>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#include <GL/glxext.h>
#endif
#endif

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

#define PLUG_THIS_PLUGIN \
    PlugRegistry::GetInstance().GetPluginWithName(TF_PP_STRINGIZE(MFB_PACKAGE_NAME))

#if defined(USD_HAS_GPU_BUFFER_SHARING)
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
#endif

namespace {

unsigned int
_GetPositionVertexCount(MGeometry* geom, int vertexBufferCount)
{
    if (!geom || vertexBufferCount <= 0) {
        return 0;
    }
    for (int vbIdx = 0; vbIdx < vertexBufferCount; ++vbIdx) {
        MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
        if (!mvb) {
            continue;
        }
        if (mvb->descriptor().semantic() == MGeometry::Semantic::kPosition) {
            return mvb->vertexCount();
        }
    }
    return 0;
}

void
_EmitRenderItemTopologyDirtyLocators(
    Fvp::DirtyNotifier& notifier,
    MHWRender::MGeometry::Primitive primitive)
{
    switch (primitive) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        notifier.dirtyMeshTopology();
        break;
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        notifier.dirtyBasisCurvesTopology();
        break;
    default:
        break;
    }
}

#if defined(USD_HAS_GPU_BUFFER_SHARING)

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
    size_t   byteStride)
{
    VtArray<T> result;
    if (rawHandle == 0 || numElements == 0) {
        return VtValue(result);
    }

    MayaResourceContextScope resourceContext;
    if (!resourceContext) {
        return VtValue(result);
    }

    const size_t elementSize = sizeof(T);
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
#endif // USD_HAS_GPU_BUFFER_SHARING

} // namespace

#if defined(USD_HAS_GPU_BUFFER_SHARING)
bool
MayaHydraRenderItemAdapter::_UseGpuBufferSharing() const
{
    // Triangles/strips -> HdMesh, lines/line-strips -> HdBasisCurves. Points
    // are intentionally excluded: point render items are not populated by
    // UpdateFromDelta yet (see its early-exit guard), so there is nothing to
    // share.
    return _IsGpuBufferSharingEnabled()
        && (_primitive == MGeometry::Primitive::kTriangles
            || _primitive == MGeometry::Primitive::kTriangleStrip
            || _primitive == MGeometry::Primitive::kLines
            || _primitive == MGeometry::Primitive::kLineStrip);
}

// The VP2 semantic of a stream, for log lines that need to say WHICH
// primvar was or was not shared.
//
// Mapped from the enum rather than taken from
// MVertexBufferDescriptor::semanticName(), which returns MString BY VALUE:
// asChar() on that temporary would dangle the moment this function
// returned. String literals have static storage, so these do not.
static const char *
_ExtStreamName(MHWRender::MVertexBuffer *mvb)
{
    if (!mvb) {
        return "<none>";
    }
    switch (mvb->descriptor().semantic()) {
    case MGeometry::Semantic::kPosition:        return "position";
    case MGeometry::Semantic::kNormal:          return "normal";
    case MGeometry::Semantic::kTexture:         return "texture";
    case MGeometry::Semantic::kColor:           return "color";
    case MGeometry::Semantic::kTangent:         return "tangent";
    case MGeometry::Semantic::kBitangent:       return "bitangent";
    case MGeometry::Semantic::kTangentWithSign: return "tangentWithSign";
    case MGeometry::Semantic::kInvalidSemantic: return "<invalid>";
    }
    return "<unknown>";
}

MayaHydraRenderItemAdapter::_ExtPublishResult
MayaHydraRenderItemAdapter::_PublishExtStream(
    MHWRender::MVertexBuffer *mvb,
    _ExtStream               &stream,
    bool                      allowDirectBind)
{
#if defined(_WIN32)
    _CaptureMayaWglContext();
#elif defined(__linux__)
    _CaptureMayaGlxContext();
#endif
    if (!mvb || mvb->vertexCount() == 0) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] no CPU fallback needed: empty vertex buffer\n",
                 GetID().GetText());
        return _ExtPublishResult::NoGpu;
    }
    void *resourceHandle = mvb->resourceHandle();
    if (!resourceHandle) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s -> CPU: VP2 gave no resource handle\n",
                 GetID().GetText(), _ExtStreamName(mvb));
        return _ExtPublishResult::NoGpu;
    }
    // resourceHandle() neither maps the buffer nor triggers a readback, so
    // identity can be checked every frame on every stream for almost nothing.
    const uint64_t rawHandle = _ExtractRawHandle(resourceHandle);
    if (rawHandle == 0) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s -> CPU: resource handle is null\n",
                 GetID().GetText(), _ExtStreamName(mvb));
        return _ExtPublishResult::NoGpu;
    }

    const _ExtLayout layout = _GetExtLayout(mvb);
    if (!layout.IsValid()) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s -> CPU: VP2 element type has no Hd equivalent "
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
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s -> CPU: no bridge (renderer Hgi: %s)\n",
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
        && stream.byteStride == layout.byteStride;

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
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
                    .Msg("[%s] %s -> CPU: %s bridge could not refresh from "
                         "buffer %" PRIu64 "\n",
                         GetID().GetText(), _ExtStreamName(mvb),
                         bridge.Describe(), rawHandle);
                return _ExtPublishResult::NoGpu;
            }
            // Only bookkeeping: which buffer the next refresh reads from.
            stream.rawHandle = rawHandle;
        }
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s unchanged: source buffer %" PRIu64 ", %zu elements "
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
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
                .Msg("[%s] %s -> CPU: %s bridge refused GL buffer %" PRIu64
                     "\n",
                     GetID().GetText(), _ExtStreamName(mvb),
                     bridge.Describe(), rawHandle);
            return _ExtPublishResult::NoGpu;
        }
    } else if (!bridge.IsZeroCopy()
                   && !bridge.Refresh(buffer, static_cast<uint32_t>(rawHandle),
                                      layout.copyByteSize)) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
            .Msg("[%s] %s -> CPU: %s bridge could not refresh from buffer "
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
    stream.allowDirectBind = allowDirectBind;

    // Logs the numbers the consumer bounds checks (byteOffset + numElements
    // * stride <= byteSize), because byteSize is derived here rather than
    // reported by Maya and getting it too small is the one failure that
    // silently disables sharing for an otherwise healthy stream.
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
        .Msg("[%s] %s SHARED via %s%s: gl=%" PRIu64 " elements=%zu "
             "offset=%zu stride=%zu byteSize=%zu allowDirectBind=%s\n",
             GetID().GetText(), _ExtStreamName(mvb), bridge.Describe(),
             bridge.IsZeroCopy() ? "" : " (copied)", rawHandle,
             layout.numElements, layout.byteOffset, layout.byteStride,
             layout.byteSize, allowDirectBind ? "yes" : "no");

    return _ExtPublishResult::Republished;
}
#endif

/*
 * MayaHydraRenderItemAdapter is used to translate from a render item to hydra.
 * This is where we translate from Maya shapes (such as meshes) to hydra using their vertex and
 * index buffers, look for "MVertexBuffer" and "MIndexBuffer" in this file to get more information.
 */
MayaHydraRenderItemAdapter::MayaHydraRenderItemAdapter(
    const MDagPath&       dagPath,
    const SdfPath&        slowId,
    int                   fastId,
    MayaHydraSceneIndex*  mayaHydraSceneIndex,
    const MRenderItem&    ri,
    TfToken              purposeRenderTag)
    : MayaHydraAdapter(dagPath.node(), slowId, mayaHydraSceneIndex)
    , _dagPath(dagPath)
    , _primitive(ri.primitive())
    , _name(ri.name())
    , _fastId(fastId)
    , _purposeRenderTag(purposeRenderTag)
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    , _cullMode(ri.cullMode())
#endif
{
}

MayaHydraRenderItemAdapter::~MayaHydraRenderItemAdapter() { _RemoveRprim(); }

void MayaHydraRenderItemAdapter::Populate()
{
    _InsertRprim(this);
}

TfToken MayaHydraRenderItemAdapter::GetRenderTag() const
{
    return _purposeRenderTag;
}

void MayaHydraRenderItemAdapter::UpdateTransform(const MRenderItem& ri)
{
    MMatrix matrix;
    if (ri.getMatrix(matrix) == MStatus::kSuccess) {
        _transform[0] = GetGfMatrixFromMaya(matrix);
        
        // _transform[1] and _transform[2] are used only when motion samples are enabled
        // so no reason to spend cycles on setting them otherwise.

        if (GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled()) {
            // MRenderItem::getMatrix() is a Viewport 2.0 snapshot of the current frame
            // and does not re-evaluate under MDGContextGuard, so sample the source DAG
            // node's world transform instead and apply those deltas to the item's matrix.
            MDagPath dag = ri.sourceDagPath();
            if (!dag.isValid()) {
                dag = _dagPath;
            }
            if (dag.isValid()) {
                const GfMatrix4d centerDag    = GetGfMatrixFromMaya(dag.inclusiveMatrix());
                const GfMatrix4d centerDagInv = centerDag.GetInverse();
                const GfInterval shutter
                    = GetMayaHydraSceneIndex()->GetCurrentTimeSamplingInterval();
                const MTime now = MAnimControl::currentTime();

                GfMatrix4d openDag;
                {
                    MDGContextGuard guard(now + shutter.GetMin());
                    openDag = GetGfMatrixFromMaya(dag.inclusiveMatrix());
                }
                GfMatrix4d closeDag;
                {
                    MDGContextGuard guard(now + shutter.GetMax());
                    closeDag = GetGfMatrixFromMaya(dag.inclusiveMatrix());
                }
                // World-space deltas, row-vector convention matching Gf and Maya:
                // worldKey = worldCentre * (centreDag^-1 * keyDag).
                _transform[1] = _transform[0] * centerDagInv * closeDag;
                _transform[2] = _transform[0] * centerDagInv * openDag;
            }
        }
    }
}

bool MayaHydraRenderItemAdapter::IsSupported() const
{
    switch (_primitive) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->basisCurves);
    case MHWRender::MGeometry::Primitive::kPoints:
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->points);
    default: return false;
    }
}

void MayaHydraRenderItemAdapter::_InsertRprim(MayaHydraAdapter* adapter)
{
    switch (GetPrimitive()) {
    case MHWRender::MGeometry::Primitive::kTriangles:
    case MHWRender::MGeometry::Primitive::kTriangleStrip:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->mesh, GetID());
        break;
    case MHWRender::MGeometry::Primitive::kLines:
    case MHWRender::MGeometry::Primitive::kLineStrip:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->basisCurves, GetID());
        break;
    case MHWRender::MGeometry::Primitive::kPoints:
        GetMayaHydraSceneIndex()->InsertPrim(adapter, HdPrimTypeTokens->points, GetID());
        break;
    default:
        TF_RUNTIME_ERROR(
            "Unsupported render item primitive %d for item '%s' (prim '%s', id '%s').",
            static_cast<int>(GetPrimitive()),
            _name.asChar(),
            _dagPath.fullPathName().asChar(),
            GetID().GetText());
        break;
    }
    _isPopulated = true;
}

void MayaHydraRenderItemAdapter::_RemoveRprim()
{
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

// We receive in that function the changes made in the Maya viewport between the last frame rendered
// and the current frame
void MayaHydraRenderItemAdapter::UpdateFromDelta(const UpdateFromDeltaData& data)
{
    if (_primitive != MHWRender::MGeometry::Primitive::kTriangles
        && _primitive != MHWRender::MGeometry::Primitive::kTriangleStrip
        && _primitive != MHWRender::MGeometry::Primitive::kLines
        && _primitive != MHWRender::MGeometry::Primitive::kLineStrip) {
        return;
    }

    const bool positionsHaveBeenReset
        = (0 == _positions.size()
#if defined(USD_HAS_GPU_BUFFER_SHARING)
           && !_extPositions
#endif
           ); // when positionsHaveBeenReset is true we need to recompute the
              // geometry and topology as our data has been cleared
    using MVS = MDataServerOperation::MViewportScene;
    // const bool isNew = flags & MViewportScene::MVS_new;  //not used yet
    const bool visible          = data._flags & MVS::MVS_visible;
    const bool matrixChanged    = data._flags & MVS::MVS_changedMatrix;
          bool geomChanged      = (data._flags & MVS::MVS_changedGeometry) || positionsHaveBeenReset;//Non const as we may modify it later => Temp workaround for a bug in Maya MAYA-134200
    const bool topoChanged      = (data._flags & MVS::MVS_changedTopo) || positionsHaveBeenReset;
    const bool visibChanged     = data._flags & MVS::MVS_changedVisibility;
    const bool effectChanged    = data._flags & MVS::MVS_changedEffect;

    // Dirty notification policy for this function - see
    // doc/render_delegate_topology_vs_deformation.md for the full contract.
    //   Granularity: emit one locator per changed datum; never use the broad primvars locator
    //     for geometry edits - that would re-pull unchanged data in the render delegate.
    //   Topology: on genuine connectivity change emit topology locators only
    //     (mesh/topology or basisCurves/topology via _EmitRenderItemTopologyDirtyLocators).
    //     When Maya also sets MVS_changedGeometry alongside MVS_changedTopo, the separate
    //     geomChanged path may dirty granular primvars (points/st/tangents and optionally normals).
    //     The broad primvars locator is NOT emitted on the topology path - it would subsume
    //     granular locators and defeat the useMayaNormals skip.
    //     Topology locators are suppressed when Maya sets MVS_changedTopo alongside
    //     MVS_changedGeometry but both vertex count and index connectivity are unchanged
    //     (deformation-only). When connectivity changes with the same vertex count, topology
    //     locators are still emitted - but only when Maya set MVS_changedTopo, because the index
    //     buffer is not read on the geometry-only path (see the Indices block below). Detecting
    //     connectivity edits therefore relies on that flag, which Maya sets for genuine ones.
    //   Extent: dirty only when the bounding box actually changes. Maya has no bbox-changed
    //     flag, so we diff the freshly-read bbox against the stored _bounds before overwriting.
    //     Checked in the geomChanged||topoChanged block (before the vertex-count workaround below),
    //     separately from the per-primvar dirty block - this is intentional, not an oversight.
    //   Normals: skip dirtyNormals() when useMayaNormals is false - Hydra generates
    //     normals itself in that mode and a redundant notification would cause unnecessary work.
    //     The guard applies on the geomChanged path where granular primvar locators are emitted.
    //
    // Construct the notifier AFTER the early-exit guard above so an early return always leaves
    // the notifier empty on an early return.
    MayaHydra::DirtyNotifier notifier(this);

#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    MRenderItem::CullMode cullMode = data._ri.cullMode();
    if (cullMode != _cullMode) {
        //  MRenderItem uses CullNone to denote doubleSided
        if (IsDoubleSided(_cullMode) || IsDoubleSided(cullMode)) {
            notifier.dirtyDoubleSided();
        }
        notifier.dirtyCullStyle();
        _cullMode = cullMode;
    }
#endif

    if (data._wireframeColorDirty) {
        // Constant-interpolation displayColor (no vertex color set present on this render item).
        notifier.dirtyDisplayColor();
    }

    const bool hideOnPlayback = data._ri.isHideOnPlayback();
    if (hideOnPlayback != _isHideOnPlayback) {
        _isHideOnPlayback = hideOnPlayback;
        notifier.dirtyVisibility();
    }

    if (visibChanged) {
        _visible = visible;
        notifier.dirtyVisibility();
    }

    if (effectChanged) {
        notifier.dirtyMaterialBinding();
    }
    if (matrixChanged) {
        notifier.dirtyTransform();
    }
    // Hoisted so it is in scope for both the topology dirty block and the per-primvar
    // geomChanged block below. The static ensures the env var is read only once.
    static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();

#if defined(USD_HAS_GPU_BUFFER_SHARING)
    // GPU buffer sharing mode + hybrid sticky-promotion classifier.
    // In hybrid mode a mesh starts batched (allowDirectBind=false) so static
    // meshes aggregate from frame one; the first real deform (geom/topo change
    // on a mesh that already published a buffer -- filtering the initial
    // populate frame) sticky-promotes it to direct mode (zero per-frame copy).
    const _GpuBufferSharingMode sharingMode = _GetGpuBufferSharingMode();
    const bool hybridMode =
        _UseGpuBufferSharing() && sharingMode == _GpuBufferSharingMode::Hybrid;
    if (hybridMode && (geomChanged || topoChanged) && _extPositions
        && !_classifiedAsAnimating) {
        _classifiedAsAnimating = true;
        // No dirty emitted here. Promotion flips useDirectBindOptimization to
        // true; on the next stream read _PublishExtStream sees the cached

        // cached allowDirectBind is now stale and republishes every
        // vertex-layer primvar (which dirties it) through the geometry-loop
        // gating below. UV/tangent (not rewritten during skeletal anim) are
        // re-fetched the same way, so the new direct-bind BAR keeps them.
    }
    // True when Storm holds a direct-bind BAR (direct mode, or hybrid after
    // promotion): the BAR keeps a stable reference to VP2's GL buffer, so a
    // byte-only deform is picked up at the next draw with no re-publish.
    const bool useDirectBindOptimization =
        _UseGpuBufferSharing()
        && (sharingMode == _GpuBufferSharingMode::Direct
            || (hybridMode && _classifiedAsAnimating));

    // Reached for every eligible render item on every update, before any
    // decision about individual streams. Silence here therefore means
    // UpdateFromDelta is not running for this prim at all -- render items are
    // not flowing -- rather than that sharing was declined.
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
        .Msg("[%s] update: sharingEnabled=%s useSharing=%s mode=%d "
             "directBind=%s geomChanged=%s topoChanged=%s\n",
             GetID().GetText(),
             _IsGpuBufferSharingEnabled() ? "yes" : "no",
             _UseGpuBufferSharing() ? "yes" : "no",
             int(sharingMode),
             useDirectBindOptimization ? "yes" : "no",
             geomChanged ? "yes" : "no",
             topoChanged ? "yes" : "no");
#endif

    // Extent is checked here, under geomChanged||topoChanged, so it is always evaluated when
    // positions or topology change - including the geomChanged case. The old code always dirtied
    // extent on geomChanged; the new code diffs the actual bbox first and only emits dirtyExtent()
    // when the value changed. This is intentional: if vertices moved without changing the bbox
    // (e.g. internal vertices shuffled), there is nothing for the render delegate to re-read.
    MGeometry* geom = nullptr;
    if (geomChanged || topoChanged) {
        geom = data._ri.geometry();
        auto bbox = data._ri.boundingBox();
        const MPoint& min = bbox.min();
        const MPoint& max = bbox.max();
        GfRange3d newRange({min.x, min.y, min.z}, {max.x, max.y, max.z});
#if defined(USD_HAS_GPU_BUFFER_SHARING)
        // GPU-buffer render items often have an empty MRenderItem bbox (vertex
        // data is GPU-only); fall back to the source shape's DAG-node bounds so
        // the prim isn't culled.
        if (newRange.IsEmpty() && _UseGpuBufferSharing()) {
            newRange = _ObjectSpaceBoundsFromDagNode(_dagPath);
        }
#endif
        if (newRange != _bounds.GetRange()) {
            notifier.dirtyExtent();
            _bounds.SetRange(newRange);
        }
        // Apply the world matrix
        MMatrix matrix;
        data._ri.getMatrix(matrix);
        _bounds.SetMatrix(GetGfMatrixFromMaya(matrix));
    }
    VtIntArray vertexIndices;
    VtIntArray vertexCounts;
        
    const int vertexBuffercount = geom ? geom->vertexBufferCount() : 0;
    const size_t storedPositionCountBeforeUpdate = _positions.size();

    //Temp workaround for a bug in Maya MAYA-134200
    if ((!geomChanged && topoChanged) && vertexBuffercount) { 
        //With face components selection, we have topoChanged which is true but geomChanged is false, but this is wrong, the number of vertices may have changed.
        //We want to check here if we also need to update the geometry if the number of vertices is different from what is stored already
        for (int vbIdx = 0; (vbIdx < vertexBuffercount) && (!geomChanged); vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if (!mvb) {
                continue;
            }

            const MVertexBufferDescriptor& desc = mvb->descriptor();
            const auto                     semantic = desc.semantic();
            switch (semantic) {
            case MGeometry::Semantic::kPosition: {
                // Vertices
                MVertexBuffer*     verts = mvb;
                const unsigned int originalVertexCount = verts->vertexCount();
                size_t storedVertexCount = _positions.size();
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                // GPU meshes keep no CPU positions; take the stored count from
                // the published external buffer instead.
                if (_extPositions) {
                    storedVertexCount = _extPositions.numElements;
                }
#endif
                if (storedVertexCount != originalVertexCount) {//Is it different ?
                    geomChanged = true;//this will stop the loop
                }
            } break;
            default: break;
            }
        }
    }

    // Per-stream dirty decisions, emitted after the geometry loop once the data
    // has actually been read/published (a dirty without a corresponding data
    // update would be a false promise to the render delegate). A CPU stream
    // dirties whenever it is re-read; a GPU-shared stream dirties only when it
    // must (see the gating in the loop): identity/mode change (Republished),
    // batch mode (re-blit every frame), or - for points - when Hydra generates
    // normals (a moved points buffer must drive the smooth-normals recompute).
    // In steady-state direct binding with a stable handle every stream stays
    // clean and Storm reads the new bytes straight from the aliased buffer.
    // (Vertex colors: set a flag here once the kColor read below is wired in.)
    bool dirtyPositions = false;
    bool dirtyNormalsStream = false;
    bool dirtyUvs = false;
    bool dirtyTangents = false;

    // Vertices
    if (geomChanged && vertexBuffercount) {
        //vertexBuffercount > 0 means geom is non null
        for (int vbIdx = 0; vbIdx < vertexBuffercount; vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if ( ! mvb) {
                continue;
            }

            const MVertexBufferDescriptor& desc = mvb->descriptor();
            const auto semantic = desc.semantic();
            switch(semantic){

                case MGeometry::Semantic::kPosition: {
                    //Vertices
                    MVertexBuffer*verts = mvb;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                    if (_UseGpuBufferSharing()) {
                        const _ExtPublishResult res = _PublishExtStream(
                            verts, _extPositions, useDirectBindOptimization);
                        if (res != _ExtPublishResult::NoGpu) {
                            _positions.clear();
                            // Points also drive Hydra's smooth-normals recompute
                            // when Maya does not supply normals, so dirty on
                            // republish, in batch mode, or whenever Hydra owns
                            // normals -- even under stable direct binding.
                            if (res == _ExtPublishResult::Republished
                                || !useDirectBindOptimization
                                || !useMayaNormals
                                || _LazyCpuBufferTriggered(HdTokens->points)) {
                                dirtyPositions = true;
                            }
                            break;
                        }
                    }
                    _extPositions = {};
#endif
                    int                vertCount = 0;
                    const unsigned int originalVertexCount = verts->vertexCount();
                    if (topoChanged) {
                        vertCount = originalVertexCount;
                    } else {
                        // Keep the previously-determined vertex count in case it was truncated.
                        const size_t positionSize = _positions.size();
                        if (positionSize > 0 && positionSize <= originalVertexCount) {
                            vertCount = positionSize;
                        } else {
                            vertCount = originalVertexCount;
                        }
                    }

                    _positions.clear();
                    const auto* vertexPositions = reinterpret_cast<const GfVec3f*>(verts->map());
                    if (TF_VERIFY(vertexPositions)) {
                        _positions.assign(vertexPositions, vertexPositions + vertCount);
                    }
                    verts->unmap();
                    dirtyPositions = true;
                }
                break;
                case MGeometry::Semantic::kNormal: {
                    //Normals
                    if (useMayaNormals){
                        MVertexBuffer* normals = mvb;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                        if (_UseGpuBufferSharing()) {
                            const _ExtPublishResult res = _PublishExtStream(
                                normals, _extNormals, useDirectBindOptimization);
                            if (res != _ExtPublishResult::NoGpu) {
                                _normals.clear();
                                if (res == _ExtPublishResult::Republished
                                    || !useDirectBindOptimization
                                    || _LazyCpuBufferTriggered(HdTokens->normals)) {
                                    dirtyNormalsStream = true;
                                }
                                break;
                            }
                        }
                        _extNormals = {};
#endif
                        int normalsCount = 0;
                        const unsigned int originalNormalsCount = normals->vertexCount();
                        if (topoChanged) {
                            normalsCount = originalNormalsCount;
                        } else {
                            // Keep the previously-determined normals count in case it was truncated.
                            const size_t normalSize = _normals.size();
                            if (normalSize > 0 && normalSize <= originalNormalsCount) {
                                normalsCount = normalSize;
                            } else {
                                normalsCount = originalNormalsCount;
                            }
                        }

                        _normals.clear();
                        const auto* vertexNormals = reinterpret_cast<const GfVec3f*>(normals->map());
                        if (TF_VERIFY(vertexNormals)) {
                            _normals.assign(vertexNormals, vertexNormals + normalsCount);
                        }
                        normals->unmap();
                        dirtyNormalsStream = true;
                    }
                }
                break;
                case MGeometry::Semantic::kTexture: {
                    // Textures:
                    if (_primitive == MGeometry::Primitive::kTriangles
                        || _primitive == MGeometry::Primitive::kTriangleStrip) {
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                        if (_UseGpuBufferSharing()) {
                            const _ExtPublishResult res = _PublishExtStream(
                                mvb, _extUvs, useDirectBindOptimization);
                            if (res != _ExtPublishResult::NoGpu) {
                                _uvs.clear();
                                if (res == _ExtPublishResult::Republished
                                    || !useDirectBindOptimization
                                    || _LazyCpuBufferTriggered(
                                        MayaHydraAdapterTokens->st)) {
                                    dirtyUvs = true;
                                }
                                break;
                            }
                        }
                        _extUvs = {};
#endif
                        int uvsCount = 0;
                        const unsigned int originalUvsCount = mvb->vertexCount();
                        if (topoChanged) {
                            uvsCount = originalUvsCount;
                        } else {
                            // Keep the previously-determined uvs count in case it was truncated.
                            const size_t uvSize = _uvs.size();
                            if (uvSize > 0 && uvSize <= originalUvsCount) {
                                uvsCount = uvSize;
                            } else {
                                uvsCount = originalUvsCount;
                            }
                        }

                        _uvs.clear();
                        const auto* uvData =
                            reinterpret_cast<const GfVec2f*>(mvb->map());
                        if (TF_VERIFY(uvData)) {
                            _uvs.assign(uvData, uvData + uvsCount);
                        }
                        mvb->unmap();
                        dirtyUvs = true;
                    }
                }
                break;
                case MHWRender::MGeometry::kTangent: {
                    // Tangents
                    if (_primitive == MGeometry::Primitive::kTriangles
                        || _primitive == MGeometry::Primitive::kTriangleStrip) {
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                        if (_UseGpuBufferSharing()) {
                            const _ExtPublishResult res = _PublishExtStream(
                                mvb, _extTangents, useDirectBindOptimization);
                            if (res != _ExtPublishResult::NoGpu) {
                                _tangents.clear();
                                if (res == _ExtPublishResult::Republished
                                    || !useDirectBindOptimization
                                    || _LazyCpuBufferTriggered(
                                        MayaHydraAdapterTokens->tangents)) {
                                    dirtyTangents = true;
                                }
                                break;
                            }
                        }
                        _extTangents = {};
#endif
                        int tangentsCount = 0;
                        const unsigned int originalTangentsCount = mvb->vertexCount();
                        if (topoChanged) {
                            tangentsCount = originalTangentsCount;
                        } else {
                            // Keep the previously-determined tangents count in case it was truncated.
                            const size_t tangentSize = _tangents.size();
                            if (tangentSize > 0 && tangentSize <= originalTangentsCount) {
                                tangentsCount = tangentSize;
                            } else {
                                tangentsCount = originalTangentsCount;
                            }
                        }

                        _tangents.clear();
                        const auto* tangentData =
                            reinterpret_cast<const GfVec3f*>(mvb->map());
                        if (TF_VERIFY(tangentData)) {
                            _tangents.assign(tangentData, tangentData + tangentsCount);
                        }
                        mvb->unmap();
                        dirtyTangents = true;
                    }
                }
                break;
                case MGeometry::Semantic::kColor:
                    // Vertex color sets (per-vertex displayColor) are not yet read from the
                    // vertex buffer. When adding support: read the buffer here and store the
                    // result, then uncomment notifier.dirtyVertexColors() in the geomChanged
                    // block above so the render delegate is notified only once data is live.
                break;
                default:
                break;
            }
        }
    }

#if defined(USD_HAS_GPU_BUFFER_SHARING)
    // A stream published earlier but absent from this geometry must be
    // withdrawn. VP2 can release a hidden item's buffers (a display-mode
    // switch does) and recycle the names, and the loop above only visits the
    // buffers that are still there -- so without this the cached schema keeps
    // naming a GL buffer VP2 no longer guarantees, and Storm copies from or
    // draws through whatever that name now is.
    if (_UseGpuBufferSharing() && (geomChanged || topoChanged)) {
        bool hasPositions = false;
        bool hasNormals = false;
        bool hasUvs = false;
        bool hasTangents = false;
        for (int vbIdx = 0; vbIdx < vertexBuffercount; vbIdx++) {
            MVertexBuffer* mvb = geom->vertexBuffer(vbIdx);
            if (!mvb) {
                continue;
            }
            switch (mvb->descriptor().semantic()) {
            case MGeometry::Semantic::kPosition: hasPositions = true; break;
            case MGeometry::Semantic::kNormal:   hasNormals = true;   break;
            case MGeometry::Semantic::kTexture:  hasUvs = true;       break;
            case MGeometry::Semantic::kTangent:  hasTangents = true;  break;
            default: break;
            }
        }
        auto withdraw = [this](_ExtStream& stream, bool present, bool& dirty,
                               const char* name) {
            if (!stream || present) {
                return;
            }
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
                .Msg("[%s] %s WITHDRAWN: gl=%" PRIu64 " is no longer in the "
                     "render item's geometry\n",
                     GetID().GetText(), name, stream.rawHandle);
            stream = {};
            dirty = true;
        };
        withdraw(_extPositions, hasPositions, dirtyPositions, "position");
        withdraw(_extNormals, hasNormals, dirtyNormalsStream, "normal");
        withdraw(_extUvs, hasUvs, dirtyUvs, "texture");
        withdraw(_extTangents, hasTangents, dirtyTangents, "tangent");
    }
#endif

    // Emit the per-primvar dirty locators decided while reading the streams
    // above. GPU-shared streams were gated in the loop, so a stable direct-bound
    // buffer stays clean (Storm reads the deformed bytes through the alias with
    // no re-pull); batch streams and identity/mode changes dirty as needed.
    if (dirtyPositions) {
        notifier.dirtyPoints();
    }
    if (dirtyUvs) {
        notifier.dirtyUVs();
    }
    if (dirtyTangents) {
        notifier.dirtyTangents();
    }
    if (dirtyNormalsStream) {
        notifier.dirtyNormals();
    }

    // Indices
    // Line strips do not make use of the index buffer, so we can skip this block.
    // See "Line strips indices are implicitly defined" comment.
    // Gated on topoChanged, never on geomChanged alone: mapping the index buffer, scanning it for
    // the highest referenced index and copying it into a VtIntArray on every deformation frame is
    // the HYDRA-2417 playback regression. Deformation leaves connectivity untouched, so on those
    // frames there is nothing here to recompute.
    if (topoChanged && vertexBuffercount
        && GetPrimitive() != MHWRender::MGeometry::Primitive::kLineStrip) {
        // Assume first stream contains the positions.
        MIndexBuffer* indices = geom->indexBuffer(0);
        if (indices) {
            int indexCount = indices->size();
            int* indicesData = (int*)indices->map();
            // USD spamming the "topology references only upto element" message is super
            // slow.  Scanning the index array to look for an incompletely used vertex
            // buffer is innefficient, but it's better than the spammy warning. Cause of
            // the incompletely used vertex buffer is unclear.  Maya scene data just is
            // that way sometimes.
            int maxIndex = 0;
            for (int i = 0; i < indexCount; i++) {
                if (indicesData[i] > maxIndex) {
                    maxIndex = indicesData[i];
                }
            }

            vertexIndices.assign(indicesData, indicesData + indexCount);

            if (maxIndex < (int64_t)_positions.size() - 1) {
                _positions.resize(maxIndex + 1);
            }
            const size_t numNormals = _normals.size();
            if (numNormals > 0 && (maxIndex < (int64_t)numNormals - 1)) {
                _normals.resize(maxIndex + 1);
            }
            const size_t numUvs = _uvs.size();
            if (numUvs > 0 && (maxIndex < (int64_t)numUvs - 1)) {
                _uvs.resize(maxIndex + 1);
            }
            const size_t numTangents = _tangents.size();
            if (numTangents > 0 && (maxIndex < (int64_t)numTangents - 1)) {
                _tangents.resize(maxIndex + 1);
            }

            switch (GetPrimitive()) {
            case MHWRender::MGeometry::Primitive::kTriangles:
                vertexCounts.resize(indexCount / 3);
                vertexCounts.assign(indexCount / 3, 3);
                break;
            case MHWRender::MGeometry::Primitive::kTriangleStrip: {
                // Convert triangle strip indices to individual triangles.
                // For N strip indices we get N-2 triangles, with alternating
                // winding to maintain consistent face orientation.
                if (indexCount >= 3) {
                    const int numTriangles = indexCount - 2;
                    vertexCounts.assign(numTriangles, 3);
                    VtIntArray expandedIndices;
                    expandedIndices.reserve(numTriangles * 3);
                    for (int i = 0; i < numTriangles; ++i) {
                        if (i % 2 == 0) {
                            expandedIndices.push_back(indicesData[i]);
                            expandedIndices.push_back(indicesData[i + 1]);
                            expandedIndices.push_back(indicesData[i + 2]);
                        } else {
                            expandedIndices.push_back(indicesData[i + 1]);
                            expandedIndices.push_back(indicesData[i]);
                            expandedIndices.push_back(indicesData[i + 2]);
                        }
                    }
                    vertexIndices = std::move(expandedIndices);
                } else {
                    vertexCounts.clear();
                    vertexIndices.clear();
                }
                break;
            }
            case MHWRender::MGeometry::Primitive::kLines:
                vertexCounts.resize(indexCount);
                vertexCounts.assign(indexCount / 2, 2);
                break;
            default:
                TF_RUNTIME_ERROR(
                    "Unsupported render item primitive %d for item '%s' (prim '%s', id '%s').",
                    static_cast<int>(GetPrimitive()),
                    _name.asChar(),
                    _dagPath.fullPathName().asChar(),
                    GetID().GetText());
                break;
            }
            indices->unmap();
        }
    }

    // Topology dirty locators are decided after index buffers are read so we can diff connectivity,
    // not just vertex count, when Maya sets topoChanged alongside geomChanged (MAYA-134200).
    const bool emitTopologyLocators = RenderItemShouldEmitTopologyLocators(
        topoChanged,
        geomChanged,
        geom && vertexBuffercount > 0,
        storedPositionCountBeforeUpdate == 0 && _positions.empty(),
        storedPositionCountBeforeUpdate,
        _GetPositionVertexCount(geom, vertexBuffercount),
        _topology.get(),
        GetPrimitive(),
        vertexIndices,
        vertexCounts);
    if (emitTopologyLocators) {
        _EmitRenderItemTopologyDirtyLocators(notifier, GetPrimitive());
    }

    // Mirrors the Indices block gate above: the cached _topology is rebuilt only when indices were
    // actually read. On a deformation frame the cached _topology is retained and stays correct,
    // because connectivity is unchanged.
    const bool indicesWereRead = topoChanged && vertexBuffercount > 0
        && GetPrimitive() != MHWRender::MGeometry::Primitive::kLineStrip;
    // Only (re)build the cached topology when connectivity actually changed
    // (== emitTopologyLocators, which already ran the connectivity diff above) or when we have no
    // cached topology yet. Maya raises MVS_changedGeometry every frame during deformation with
    // unchanged connectivity; rebuilding on those frames would rescan the whole face-vertex index
    // array (HdMeshTopology::ComputeNumPoints) and reallocate for nothing - no topology-dirty
    // locator is emitted on those frames, so Storm never re-pulls the rebuilt copy.
    const bool topologyNeedsRebuild = emitTopologyLocators || !_topology;
    if (indicesWereRead && !vertexCounts.empty() && topologyNeedsRebuild) {
        switch (GetPrimitive()) {
        case MGeometry::Primitive::kTriangleStrip:
        case MGeometry::Primitive::kTriangles: {
            if (useMayaNormals) {
                _topology.reset(new HdMeshTopology(
                    PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            } else {
                _topology.reset(new HdMeshTopology(
                    (GetMayaHydraSceneIndex()->GetParams().displaySmoothMeshes
                     || GetDisplayStyle().refineLevel > 0)
                        ? PxOsdOpenSubdivTokens->catmullClark
                        : PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    vertexCounts,
                    vertexIndices));
            }
            break;
        }
        case MGeometry::Primitive::kLines: {
            _topology.reset(new HdBasisCurvesTopology(
                HdTokens->linear,
                {},
                HdTokens->segmented,
                vertexCounts,
                vertexIndices));
            break;
        }
        default: break;
        }
    } else if (topoChanged) {
        switch (GetPrimitive()) {
        case MGeometry::Primitive::kTriangleStrip:
        case MGeometry::Primitive::kTriangles:
            if (vertexCounts.empty()) {
                _topology.reset();
            }
            break;
        case MGeometry::Primitive::kLines:
        case MGeometry::Primitive::kLineStrip: {
            TfToken curveTopoType(HdTokens->segmented);
            if (GetPrimitive() == MGeometry::Primitive::kLineStrip) {
                // Line strips indices are implicitly defined:
                // When using line strips, the GPU will draw a connected series of lines between the
                // vertices specified by the indices. When specifying indices for a line strip, you
                // only need to specify the order of the vertices that you want connected. This is
                // implicit in Hydra when specifying an empty index buffer.
                curveTopoType = HdTokens->nonperiodic;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
                // In GPU buffer sharing mode _positions is cleared (the data
                // lives only in the shared buffer), so take the vertex count
                // from the published ext schema instead of the empty CPU array.
                const size_t lineStripVertexCount = _extPositions
                    ? _extPositions.numElements
                    : _positions.size();
#else
                const size_t lineStripVertexCount = _positions.size();
#endif
                vertexCounts.assign(1, lineStripVertexCount);
                vertexIndices = VtIntArray();
            }
            _topology.reset(new HdBasisCurvesTopology(
                HdTokens->linear,
                // basis type is ignored, due to linear curve type
                {},
                curveTopoType,
                vertexCounts,
                vertexIndices));
            break;
        }
        default: break;
        }
    }
}

HdMeshTopology MayaHydraRenderItemAdapter::GetMeshTopology()
{
    return _topology ? *static_cast<HdMeshTopology*>(_topology.get()) : HdMeshTopology();
}

HdBasisCurvesTopology MayaHydraRenderItemAdapter::GetBasisCurvesTopology()
{
    return _topology ? *static_cast<HdBasisCurvesTopology*>(_topology.get())
                     : HdBasisCurvesTopology();
}

#if defined(USD_HAS_GPU_BUFFER_SHARING)
HdContainerDataSourceHandle
MayaHydraRenderItemAdapter::GetExtGpuBufferSchema(const TfToken& key) const
{
    if (key == HdTokens->points)                 { return _extPositions.schema; }
    if (key == HdTokens->normals)                { return _extNormals.schema; }
    if (key == MayaHydraAdapterTokens->tangents) { return _extTangents.schema; }
    if (key == MayaHydraAdapterTokens->st)       { return _extUvs.schema; }
    return nullptr;
}

bool
MayaHydraRenderItemAdapter::_LazyCpuBufferTriggered(const TfToken& key) const
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
MayaHydraRenderItemAdapter::_SetLazyCpuBufferTriggered(
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
        _lazyCpuBufferTriggeredMask.fetch_or(
            bit, std::memory_order_relaxed);
    }
}

VtValue
MayaHydraRenderItemAdapter::GetExtGpuBufferLazyValue(const TfToken& key) const
{
    // The adapter is shared by all viewports. Once any consumer pulls this
    // CPU fallback, future changes to that primvar must be dirtied globally;
    // GPU-capable renderers still take extGpuBuffer and avoid this map.
    _SetLazyCpuBufferTriggered(key);

    if (key == HdTokens->points) {
        return _GetExtVertexBufferValue<GfVec3f>(
            _extPositions.rawHandle,
            _extPositions.numElements,
            _extPositions.byteOffset,
            _extPositions.byteStride);
    }
    if (key == HdTokens->normals) {
        return _GetExtVertexBufferValue<GfVec3f>(
            _extNormals.rawHandle,
            _extNormals.numElements,
            _extNormals.byteOffset,
            _extNormals.byteStride);
    }
    if (key == MayaHydraAdapterTokens->tangents) {
        return _GetExtVertexBufferValue<GfVec3f>(
            _extTangents.rawHandle,
            _extTangents.numElements,
            _extTangents.byteOffset,
            _extTangents.byteStride);
    }
    if (key == MayaHydraAdapterTokens->st) {
        return _GetExtVertexBufferValue<GfVec2f>(
            _extUvs.rawHandle,
            _extUvs.numElements,
            _extUvs.byteOffset,
            _extUvs.byteStride);
    }
    return {};
}
#endif

VtValue MayaHydraRenderItemAdapter::Get(const TfToken& key)
{
    if (key == HdTokens->points) {
        return VtValue(_positions);
    }
    if (key == HdTokens->normals) {
        return VtValue(_normals);
    }
    if (key == MayaHydraAdapterTokens->tangents){
        return VtValue(_tangents);
    }
    if (key == MayaHydraAdapterTokens->st) {
        return VtValue(_uvs);
    }
    if (key == HdTokens->displayColor) {
        return VtValue(GfVec4f(
            _wireframeColor[0], _wireframeColor[1], _wireframeColor[2], _wireframeColor[3]));
    }

    // Let base class handle other keys
    return MayaHydraAdapter::Get(key);
}

HdPrimvarDescriptorVector
MayaHydraRenderItemAdapter::GetPrimvarDescriptors(HdInterpolation interpolation)
{
    // Base descriptors
    HdPrimvarDescriptorVector descs = MayaHydraAdapter::GetPrimvarDescriptors(interpolation);

    // Local descriptors
    HdPrimvarDescriptorVector localDescs;
    if (interpolation == HdInterpolationVertex) {// Vertices
        static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();
        if(useMayaNormals) {
            localDescs = {
                { UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point },//Vertices
                { UsdGeomTokens->normals, interpolation, HdPrimvarRoleTokens->normal }//Normals
            };
        }
        else {
            localDescs = {
                { UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point }//Vertices only
            };
        }
        // Also use HdInterpolationVertex for UV/Tangent, same as Normal
        // The vertex buffers in MRenderItem was already expanded as per-face-vertex
        // E.g., A default Maya cube polygon mesh will give 24 face-vertices/normals/uvs/tangents vertex buffers
        // Note: the default cube doesn't give 36 face vertices as VP2 deduplicated them.
        if (_primitive == MGeometry::Primitive::kTriangles
            || _primitive == MGeometry::Primitive::kTriangleStrip) {
            localDescs.push_back(
                {MayaHydraAdapterTokens->st, interpolation, HdPrimvarRoleTokens->textureCoordinate}); //uvs
            localDescs.push_back(
                {MayaHydraAdapterTokens->tangents, interpolation, HdPrimvarRoleTokens->textureCoordinate}); //tangents
        }
    } else if (interpolation == HdInterpolationConstant) {
        switch(_primitive){
            case MGeometry::Primitive::kPoints: //Fall into
            case MGeometry::Primitive::kLines: //Fall into
            case MGeometry::Primitive::kLineStrip: //Fall into
            case MGeometry::Primitive::kAdjacentLines: //Fall into
            case MGeometry::Primitive::kAdjacentLineStrip:
            {
                localDescs = { { HdTokens->displayColor, interpolation, HdPrimvarRoleTokens->color } };//Use display color only for lines/points (avoid triangles)
            }
            break;
            default:
            break;
        }
    }

    // Combine descriptors
    descs.insert(descs.end(), localDescs.begin(), localDescs.end());
    return descs;
}

VtValue MayaHydraRenderItemAdapter::GetMaterialResource() { return {}; }

bool MayaHydraRenderItemAdapter::GetVisible()
{
    // Assuming that, if the playback is in the active view only
    // (MAnimControl::kPlaybackViewActive), we are called because we are in the active view
    if (_isHideOnPlayback && _isInPlayback) {
        return false;
    }

    if (!_wireframeSelectionHighlightEnabled) {
        return false;
    }

    return _visible;
}

void MayaHydraRenderItemAdapter::SetWireframeSelectionHighlightEnabled(bool enabled)
{
    if (_wireframeSelectionHighlightEnabled != enabled) {
        _wireframeSelectionHighlightEnabled = enabled;
        MayaHydra::DirtyNotifier(this).dirtyVisibility();
    }
}

void MayaHydraRenderItemAdapter::SetPlaybackState(bool isPlaybackRunning)
{
    // There was a change in the playblack state, it started or stopped running so update any
    // primitive that is dependent on this
    if (_isInPlayback != isPlaybackRunning) {
        _isInPlayback = isPlaybackRunning;
        if (_isHideOnPlayback) {
            MayaHydra::DirtyNotifier(this).dirtyVisibility();
        }
    }
}

HdCullStyle MayaHydraRenderItemAdapter::GetCullStyle() const
{
    if (_isArnoldSkyDomeLightTriangleShape) {
        return HdCullStyleFront;
    }
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    switch (_cullMode) {
    case MRenderItem::CullNone: return HdCullStyleNothing;
    case MRenderItem::CullFront: return HdCullStyleFront;
    case MRenderItem::CullBack: return HdCullStyleBack;
    default: return HdCullStyleNothing;
    }
#else
    return HdCullStyleNothing;
#endif
}

bool MayaHydraRenderItemAdapter::Illuminated() const
{
    // Special case to recognize the Arnold skydome light
    if ((_isArnoldSkyDomeLightTriangleShape)) {
        return false; // Don't light the sky dome light shape
    }

    return (
        MHWRender::MGeometry::Primitive::kLines != _primitive
        && MHWRender::MGeometry::Primitive::kLineStrip != _primitive
        && MHWRender::MGeometry::Primitive::kPoints != _primitive);
}

void MayaHydraRenderItemAdapter::CreateCallbacks()
{
    MStatus status;
    auto obj = GetNode();
    auto attributesChanged = MNodeMessage::addAttributeChangedCallback(
        obj,
        +[](MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* clientData) {
            auto* adapter = reinterpret_cast<MayaHydraRenderItemAdapter*>(clientData);
            TF_UNUSED(otherPlug);
            if (!MayaHydraAdapter::AttributeMessageAffectsExtensionPrimvars(msg)) {
                return;
            }
            MObject node = adapter->GetNode();
            // Skip extension/dynamic primvars on camera/light render items to avoid duplicate
            // dirty notifications alongside the sprim updates.
            if (node.hasFn(MFn::kCamera) || node.hasFn(MFn::kLight)) {
                return;
            }
            adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
        },
        reinterpret_cast<void*>(this),
        &status);

    if (status) {
        AddCallback(attributesChanged);
    }
}

///////////////////////////////////////////////////////////////////////
// TF_REGISTRY
///////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraRenderItemAdapter, TfType::Bases<MayaHydraAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, renderItem) { }

PXR_NAMESPACE_CLOSE_SCOPE
