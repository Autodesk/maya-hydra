// Copyright 2026 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "renderThreadContext.h"

// windows.h first, for HDC/HGLRC and the wgl* entry points, which live in
// opengl32 rather than in garch.
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

#include <pxr/imaging/garch/glApi.h>

#include <cstdio>

#if defined(_WIN32)

typedef HGLRC(WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002

#endif // _WIN32

namespace MAYAHYDRA_NS_DEF
{

namespace
{

#if defined(_WIN32)

const char* const kWindowClassName = "MayaHydraRenderThreadContext";

std::string _Win32Error(const char* what)
{
    char buf[160];
    std::snprintf(buf, sizeof(buf), "%s failed, GetLastError=%lu", what, GetLastError());
    return buf;
}

/// The GL context the render thread owns, drawing to a 1x1 hidden window of its
/// own rather than to a Maya viewport, whose device context goes stale if Maya
/// recreates that window (renderer switch, panel teardown) while the thread has
/// it current. Creation can fail legitimately, since the legacy
/// PIXELFORMATDESCRIPTOR from DescribePixelFormat cannot express everything
/// WGL_ARB_pixel_format can; the caller then renders synchronously.
class GlRenderThreadContext final : public RenderThreadContext
{
public:
    ~GlRenderThreadContext() override
    {
        DestroyContext();
        _DestroySurface();
    }

    bool CreateShared(const RenderThreadHostContext& host) override
    {
        if (!host.IsValid()) {
            _lastError = "the host context is invalid; CaptureCurrentHostContext must be "
                         "called on the main thread while Maya's GL context is current";
            return false;
        }

        if (!_CreateSurface(static_cast<HDC>(host.nativeSurface))) {
            return false;
        }
        if (!_CreateContext(static_cast<HGLRC>(host.nativeContext))) {
            _DestroySurface();
            return false;
        }
        return true;
    }

    bool MakeCurrent() override
    {
        if (!_ctx || !_dc) {
            _lastError = "MakeCurrent called before a context was created";
            return false;
        }
        if (!wglMakeCurrent(_dc, _ctx)) {
            _lastError = _Win32Error("wglMakeCurrent");
            return false;
        }
        // Entry points are loaded process-wide, but load here too: if the
        // loader has not run yet, every later GL call is a null dereference.
        if (!GarchGLApiLoad()) {
            _lastError = "GarchGLApiLoad failed on the render thread context";
            wglMakeCurrent(nullptr, nullptr);
            return false;
        }
        return true;
    }

    void ReleaseCurrent() override { wglMakeCurrent(nullptr, nullptr); }

    void DestroyContext() override
    {
        if (_ctx) {
            wglDeleteContext(_ctx);
            _ctx = nullptr;
        }
    }

    const std::string& LastError() const override { return _lastError; }

private:
    bool _CreateContext(HGLRC shareWith)
    {
        // Share at creation rather than via wglShareLists, which requires the
        // new context own no objects yet and is driver-dependent when called
        // with another context current - as one is, inside Maya's draw.
        auto createContextAttribs = reinterpret_cast<PFNWGLCREATECONTEXTATTRIBSARBPROC>(
            wglGetProcAddress("wglCreateContextAttribsARB"));

        if (createContextAttribs != nullptr) {
            const int attribs[] = {
                WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
                WGL_CONTEXT_MINOR_VERSION_ARB, 5,
                WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB,
                0
            };
            _ctx = createContextAttribs(_dc, shareWith, attribs);
            if (_ctx) {
                return true;
            }
            _lastError = _Win32Error("wglCreateContextAttribsARB(4.5 compat, shared)");
        } else {
            _lastError = "wglCreateContextAttribsARB is unavailable";
        }

        // Fallback: an unshared context plus wglShareLists.
        _ctx = wglCreateContext(_dc);
        if (!_ctx) {
            _lastError = _Win32Error("wglCreateContext");
            return false;
        }
        if (!wglShareLists(shareWith, _ctx)) {
            _lastError = _Win32Error("wglShareLists");
            wglDeleteContext(_ctx);
            _ctx = nullptr;
            return false;
        }
        return true;
    }

    bool _CreateSurface(HDC hostDc)
    {
        // Copy Maya's pixel format so the shared context is compatible with
        // both surfaces. Without this the context creates but cannot be made
        // current.
        const int hostFormat = GetPixelFormat(hostDc);
        if (hostFormat == 0) {
            _lastError = _Win32Error("GetPixelFormat on Maya's device context");
            return false;
        }

        PIXELFORMATDESCRIPTOR pfd {};
        if (DescribePixelFormat(hostDc, hostFormat, sizeof(pfd), &pfd) == 0) {
            _lastError = _Win32Error("DescribePixelFormat");
            return false;
        }

        // DefWindowProcA lives in user32, so the class stays valid if this
        // library is unloaded and loaded again at another address.
        WNDCLASSA wc {};
        wc.lpfnWndProc   = DefWindowProcA;
        wc.hInstance     = GetModuleHandleA(nullptr);
        wc.lpszClassName = kWindowClassName;
        // A duplicate registration is expected on the second context and is not
        // an error; CreateWindowExA below reports real failure.
        RegisterClassA(&wc);

        _hwnd = CreateWindowExA(0, kWindowClassName, "MayaHydra render thread", 0,
                                0, 0, 1, 1, nullptr, nullptr, wc.hInstance, nullptr);
        if (!_hwnd) {
            _lastError = _Win32Error("CreateWindowExA for the render thread surface");
            return false;
        }

        _dc = GetDC(_hwnd);
        if (!_dc) {
            _lastError = _Win32Error("GetDC on the render thread surface");
            _DestroySurface();
            return false;
        }

        // Match by descriptor rather than by index: format indices are
        // per-device-context and Maya's index means nothing on our window.
        const int ourFormat = ChoosePixelFormat(_dc, &pfd);
        if (ourFormat == 0 || !SetPixelFormat(_dc, ourFormat, &pfd)) {
            _lastError = _Win32Error("ChoosePixelFormat/SetPixelFormat matching Maya's format");
            _DestroySurface();
            return false;
        }
        return true;
    }

    void _DestroySurface()
    {
        if (_dc && _hwnd) {
            ReleaseDC(_hwnd, _dc);
        }
        _dc = nullptr;
        if (_hwnd) {
            DestroyWindow(_hwnd);
            _hwnd = nullptr;
        }
    }

    HWND        _hwnd { nullptr };
    HDC         _dc { nullptr };
    HGLRC       _ctx { nullptr };
    std::string _lastError;
};

#endif // _WIN32

} // anonymous namespace

RenderThreadHostContext CaptureCurrentHostContext()
{
    RenderThreadHostContext host;
#if defined(_WIN32)
    host.nativeContext = wglGetCurrentContext();
    host.nativeSurface = wglGetCurrentDC();
#endif
    return host;
}

std::unique_ptr<RenderThreadContext> CreateGlRenderThreadContext()
{
#if defined(_WIN32)
    return std::make_unique<GlRenderThreadContext>();
#else
    // No implementation yet; null makes the caller fall back to the
    // synchronous path.
    return nullptr;
#endif
}

} // namespace MAYAHYDRA_NS_DEF
