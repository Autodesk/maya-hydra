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
//
// The graphics context the render thread owns.
//
// A context created in Maya's share group and made current on a MayaHydra-owned
// thread shares buffers, textures, programs and sync objects with Maya's
// context, but not container objects such as framebuffers and vertex arrays.
// A GL context belongs to one thread at a time, so from MakeCurrent until
// DestroyContext it sits on the render thread. Its drawing surface, a hidden
// window on Windows, which Win32 ties to the thread that created it, and a
// pbuffer on Linux, is created and destroyed on the main thread.

#ifndef MAYAHYDRALIB_RENDER_THREAD_CONTEXT_H
#define MAYAHYDRALIB_RENDER_THREAD_CONTEXT_H

#include <mayaHydraLib/api.h>

#include <memory>
#include <string>

namespace MAYAHYDRA_NS_DEF
{

/// The host (Maya) context to share with. Two handles because WGL cannot create
/// or make current a context without a device context, nor GLX without the X
/// display connection.
struct RenderThreadHostContext
{
    void* nativeContext = nullptr; ///< HGLRC on Windows, GLXContext on Linux.
    void* nativeSurface = nullptr; ///< HDC on Windows, Display* on Linux.

    bool IsValid() const { return nativeContext != nullptr && nativeSurface != nullptr; }
};

/// Captures the host context bound on the calling thread. Call from inside the
/// render override's Render(); outside the draw the result is invalid.
MAYAHYDRALIB_API
RenderThreadHostContext CaptureCurrentHostContext();

class MAYAHYDRALIB_API RenderThreadContext
{
public:
    /// Main thread. Destroys the surface, and the context if DestroyContext
    /// was not called.
    virtual ~RenderThreadContext() = default;

    /// Create a context sharing objects with \p host. Main thread.
    virtual bool CreateShared(const RenderThreadHostContext& host) = 0;

    /// Bind the context to the calling thread. Render thread only.
    virtual bool MakeCurrent() = 0;

    /// Unbind. Render thread only, and required before DestroyContext.
    virtual void ReleaseCurrent() = 0;

    /// Delete the context, leaving the surface. Render thread only, after
    /// ReleaseCurrent.
    virtual void DestroyContext() = 0;

    /// Last failure, empty when none.
    virtual const std::string& LastError() const = 0;
};

/// GL implementation. Returns null on platforms without one.
MAYAHYDRALIB_API
std::unique_ptr<RenderThreadContext> CreateGlRenderThreadContext();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_RENDER_THREAD_CONTEXT_H
