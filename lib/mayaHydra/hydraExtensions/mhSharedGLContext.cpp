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

// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include "mhSharedGLContext.h"

#include <pxr/base/tf/diagnostic.h>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__linux__)
#include <GL/glx.h>
#include <GL/glxext.h>
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

struct MhSharedGLContext::_Native
{
#if defined(_WIN32)
    HWND  window = nullptr;
    HDC   dc = nullptr;
    HGLRC context = nullptr;
#elif defined(__linux__)
    Display*   display = nullptr;
    GLXPbuffer pbuffer = 0;
    GLXContext context = nullptr;
#endif

    ~_Native()
    {
#if defined(_WIN32)
        if (context) {
            wglDeleteContext(context);
        }
        if (dc && window) {
            ReleaseDC(window, dc);
        }
        if (window) {
            DestroyWindow(window);
        }
#elif defined(__linux__)
        if (context && display) {
            glXDestroyContext(display, context);
        }
        if (pbuffer && display) {
            glXDestroyPbuffer(display, pbuffer);
        }
#endif
    }
};

// What was current on the thread before the Scope, restored on exit.
struct MhSharedGLContext::Scope::_Previous
{
#if defined(_WIN32)
    HDC   dc = nullptr;
    HGLRC context = nullptr;
#elif defined(__linux__)
    Display*    display = nullptr;
    GLXDrawable draw = None;
    GLXDrawable read = None;
    GLXContext  context = nullptr;
    // Ours, to release with when nothing was current before.
    Display* ownDisplay = nullptr;
#endif
};

namespace {

#if defined(__linux__)

using _GlxCreateContextAttribsArbProc
    = GLXContext (*)(Display*, GLXFBConfig, GLXContext, Bool, const int*);

#elif defined(_WIN32)

using _WglCreateContextAttribsArbProc = HGLRC(WINAPI*)(HDC, HGLRC, const int*);

constexpr wchar_t _WindowClassName[] = L"MayaHydraSharedWglContextWindow";

bool _RegisterWindowClass()
{
    static const bool registered = []() {
        WNDCLASSW windowClass {};
        windowClass.style = CS_OWNDC;
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = _WindowClassName;
        if (!RegisterClassW(&windowClass) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
            TF_RUNTIME_ERROR(
                "Could not register the MayaHydra WGL window class (error %lu)",
                static_cast<unsigned long>(GetLastError()));
            return false;
        }
        return true;
    }();
    return registered;
}

#endif

} // namespace

#if defined(_WIN32)

// One hidden window with Maya's pixel format and a context sharing Maya's
// objects. Null, with a warning, on failure.
std::unique_ptr<MhSharedGLContext::_Native> MhSharedGLContext::_CreateNativeFromCurrent()
{
    const HGLRC mayaContext = wglGetCurrentContext();
    const HDC   mayaDc = wglGetCurrentDC();
    if (!mayaContext || !mayaDc) {
        return nullptr;
    }

    // Needs Maya's context current, which was just checked.
    const auto createContextAttribs = reinterpret_cast<_WglCreateContextAttribsArbProc>(
        wglGetProcAddress("wglCreateContextAttribsARB"));
    if (!createContextAttribs) {
        TF_WARN("wglCreateContextAttribsARB is unavailable");
        return nullptr;
    }
    if (!_RegisterWindowClass()) {
        return nullptr;
    }

    const HWND window = CreateWindowExW(
        0,
        _WindowClassName,
        L"",
        WS_POPUP,
        0,
        0,
        1,
        1,
        nullptr,
        nullptr,
        GetModuleHandleW(nullptr),
        nullptr);
    if (!window) {
        TF_WARN(
            "Could not create a MayaHydra WGL window (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return nullptr;
    }

    auto native = std::make_unique<MhSharedGLContext::_Native>();
    native->window = window;
    native->dc = GetDC(window);

    const int             pixelFormat = GetPixelFormat(mayaDc);
    PIXELFORMATDESCRIPTOR pfd {};
    if (!native->dc || pixelFormat == 0
        || !DescribePixelFormat(mayaDc, pixelFormat, sizeof(pfd), &pfd)
        || !SetPixelFormat(native->dc, pixelFormat, &pfd)) {
        TF_WARN(
            "Could not copy Maya's pixel format to a MayaHydra WGL window (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return nullptr; // ~_Native releases the DC and destroys the window
    }

    constexpr int WglContextMajorVersionArb = 0x2091;
    constexpr int WglContextMinorVersionArb = 0x2092;
    constexpr int WglContextProfileMaskArb = 0x9126;
    constexpr int WglContextCompatibilityProfileBitArb = 0x00000002;
    const int     attribs[] = { WglContextMajorVersionArb,
                                4,
                                WglContextMinorVersionArb,
                                5,
                                WglContextProfileMaskArb,
                                WglContextCompatibilityProfileBitArb,
                                0 };

    SetLastError(ERROR_SUCCESS);
    native->context = createContextAttribs(native->dc, mayaContext, attribs);
    if (!native->context) {
        TF_WARN(
            "Could not create a WGL context sharing Maya's objects (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return nullptr;
    }
    return native;
}

#elif defined(__linux__)

// One pbuffer from Maya's framebuffer configuration and a context sharing
// Maya's objects. Null, with a warning, on failure.
std::unique_ptr<MhSharedGLContext::_Native> MhSharedGLContext::_CreateNativeFromCurrent()
{
    Display* const   display = glXGetCurrentDisplay();
    const GLXContext mayaContext = glXGetCurrentContext();
    if (!display || !mayaContext) {
        return nullptr;
    }

    const auto createContextAttribs = reinterpret_cast<_GlxCreateContextAttribsArbProc>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
    if (!createContextAttribs) {
        TF_WARN("glXCreateContextAttribsARB is unavailable");
        return nullptr;
    }

    int fbConfigId = 0;
    int screen = 0;
    if (glXQueryContext(display, mayaContext, GLX_FBCONFIG_ID, &fbConfigId) != Success
        || glXQueryContext(display, mayaContext, GLX_SCREEN, &screen) != Success) {
        TF_WARN("Could not query Maya's GLX context configuration");
        return nullptr;
    }
    const int    fbConfigAttribs[] = { GLX_FBCONFIG_ID, fbConfigId, None };
    int          fbConfigCount = 0;
    GLXFBConfig* fbConfigs = glXChooseFBConfig(display, screen, fbConfigAttribs, &fbConfigCount);
    if (!fbConfigs || fbConfigCount == 0) {
        if (fbConfigs) {
            XFree(fbConfigs);
        }
        TF_WARN("Could not find Maya's GLX framebuffer configuration");
        return nullptr;
    }
    const GLXFBConfig fbConfig = fbConfigs[0];
    XFree(fbConfigs);

    auto native = std::make_unique<MhSharedGLContext::_Native>();
    native->display = display;

    const int pbufferAttribs[] = { GLX_PBUFFER_WIDTH, 1, GLX_PBUFFER_HEIGHT, 1, None };
    native->pbuffer = glXCreatePbuffer(display, fbConfig, pbufferAttribs);
    if (!native->pbuffer) {
        TF_WARN("Could not create a MayaHydra GLX pbuffer");
        return nullptr;
    }

    const int contextAttribs[] = { GLX_CONTEXT_MAJOR_VERSION_ARB,
                                   4,
                                   GLX_CONTEXT_MINOR_VERSION_ARB,
                                   5,
                                   GLX_CONTEXT_PROFILE_MASK_ARB,
                                   GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                                   None };
    native->context
        = createContextAttribs(display, fbConfig, mayaContext, True, contextAttribs);
    if (!native->context) {
        TF_WARN("Could not create a GLX context sharing Maya's objects");
        return nullptr; // ~_Native destroys the pbuffer
    }
    return native;
}

#else

std::unique_ptr<MhSharedGLContext::_Native> MhSharedGLContext::_CreateNativeFromCurrent()
{
    return nullptr;
}

#endif

std::unique_ptr<MhSharedGLContext> MhSharedGLContext::CreateFromCurrent()
{
    void* const shareContext = GetCurrentNativeContext();
    if (!shareContext) {
        return nullptr;
    }
    std::unique_ptr<_Native> native = _CreateNativeFromCurrent();
    if (!native) {
        return nullptr;
    }
    return std::unique_ptr<MhSharedGLContext>(
        new MhSharedGLContext(std::move(native), shareContext));
}

void* MhSharedGLContext::GetCurrentNativeContext()
{
#if defined(_WIN32)
    return wglGetCurrentContext();
#elif defined(__linux__)
    return glXGetCurrentContext();
#else
    return nullptr;
#endif
}

MhSharedGLContext::MhSharedGLContext(std::unique_ptr<_Native> native, void* shareContext)
    : _native(std::move(native))
    , _shareContext(shareContext)
{
}

MhSharedGLContext::~MhSharedGLContext() = default;

MhSharedGLContext::Scope::Scope(const MhSharedGLContext& context)
    : _previous(std::make_unique<_Previous>())
{
    const _Native& native = *context._native;
#if defined(_WIN32)
    _previous->dc = wglGetCurrentDC();
    _previous->context = wglGetCurrentContext();
    if (!wglMakeCurrent(native.dc, native.context)) {
        TF_RUNTIME_ERROR(
            "Could not make a MayaHydra shared GL context current (error %lu)",
            static_cast<unsigned long>(GetLastError()));
        return;
    }
#elif defined(__linux__)
    _previous->display = glXGetCurrentDisplay();
    _previous->draw = glXGetCurrentDrawable();
    _previous->read = glXGetCurrentReadDrawable();
    _previous->context = glXGetCurrentContext();
    _previous->ownDisplay = native.display;
    if (!glXMakeContextCurrent(native.display, native.pbuffer, native.pbuffer, native.context)) {
        TF_RUNTIME_ERROR("Could not make a MayaHydra shared GL context current");
        return;
    }
#else
    (void)native;
    return;
#endif
    _isCurrent = true;
}

MhSharedGLContext::Scope::~Scope()
{
    // Restores the previous context, or makes none current if there was none:
    // a context left current on one thread cannot be made current on another.
    if (!_isCurrent) {
        return;
    }
#if defined(_WIN32)
    if (_previous->context) {
        TF_VERIFY(
            wglMakeCurrent(_previous->dc, _previous->context),
            "Could not restore the thread's previous WGL context");
    } else {
        TF_VERIFY(
            wglMakeCurrent(nullptr, nullptr), "Could not release the MayaHydra shared GL context");
    }
#elif defined(__linux__)
    if (_previous->context && _previous->display) {
        TF_VERIFY(
            glXMakeContextCurrent(
                _previous->display, _previous->draw, _previous->read, _previous->context),
            "Could not restore the thread's previous GLX context");
    } else {
        TF_VERIFY(
            glXMakeContextCurrent(_previous->ownDisplay, None, None, nullptr),
            "Could not release the MayaHydra shared GL context");
    }
#endif
}

} // namespace MAYAHYDRA_NS_DEF
