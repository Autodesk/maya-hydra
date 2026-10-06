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

#ifndef LIB_MAYAHYDRA_HYDRAEXTENSIONS_MHSHAREDGLCONTEXT_H
#define LIB_MAYAHYDRA_HYDRAEXTENSIONS_MHSHAREDGLCONTEXT_H

#include <mayaHydraLib/api.h>

#include <memory>

namespace MAYAHYDRA_NS_DEF {

/// A hidden GL context that shares objects (buffers, textures, sync objects)
/// with Maya's viewport context, for issuing GL from threads other than the
/// one Maya's context is current on.
///
/// Sharing objects is not sharing a command stream: commands issued here are
/// not ordered against Maya's. A reader of what Maya wrote must synchronize
/// explicitly, for example by waiting on a fence placed in Maya's stream.
///
/// The context owns its own drawable -- a hidden 1x1 window with Maya's pixel
/// format on Windows, a 1x1 pbuffer from Maya's framebuffer configuration on
/// Linux -- so it never competes for Maya's. It is a 4.5 compatibility-profile
/// context, which the direct-state-access buffer calls need. macOS is not
/// supported: Maya's viewport there runs on Metal.
///
/// The object makes no attempt to serialize its use. A GL context can be
/// current on one thread at a time, so callers either guard a shared instance
/// with a lock or give each thread its own.
///
/// Lifetime rules:
/// - Create on the main thread with Maya's context current.
/// - Destroy on the thread that created it (Windows destroys the window
///   there), and only when no Scope holds it current on any thread.
/// - Maya renders every VP2 panel with one context, owned by a hidden
///   resource window, and keeps it until the application shuts down; panels
///   with their own context (stereo) share objects with it. So one instance,
///   created once, serves every panel. If the share group ever did go away,
///   GL names created through this context would be freed with it.
class MhSharedGLContext
{
public:
    /// Create a context sharing with the GL context current on the calling
    /// thread. Main thread, Maya's context current. Returns null, with a
    /// warning, when creation fails, and null without one when no context is
    /// current or the platform is unsupported.
    MAYAHYDRALIB_API
    static std::unique_ptr<MhSharedGLContext> CreateFromCurrent();

    /// The native context current on the calling thread (HGLRC on Windows,
    /// GLXContext on Linux), or null. Comparing it against
    /// GetShareContext() tells whether Maya's context is still the one this
    /// instance shares with.
    MAYAHYDRALIB_API
    static void* GetCurrentNativeContext();

    MAYAHYDRALIB_API
    ~MhSharedGLContext();

    MhSharedGLContext(const MhSharedGLContext&) = delete;
    MhSharedGLContext& operator=(const MhSharedGLContext&) = delete;

    /// The native context this one shares objects with.
    void* GetShareContext() const { return _shareContext; }

    /// Makes the context current on the calling thread for the scope, and
    /// restores whatever was current before on exit -- including separate
    /// draw and read drawables on Linux -- or makes none current if there was
    /// none. Converts to false when the context could not be made current; the
    /// caller must then not issue GL.
    class Scope
    {
    public:
        MAYAHYDRALIB_API
        explicit Scope(const MhSharedGLContext& context);

        MAYAHYDRALIB_API
        ~Scope();

        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;

        explicit operator bool() const { return _isCurrent; }

    private:
        struct _Previous;
        std::unique_ptr<_Previous> _previous;
        bool                       _isCurrent = false;
    };

private:
    struct _Native;

    // Platform part of CreateFromCurrent: the context and drawable, sharing
    // with the context current on the calling thread.
    static std::unique_ptr<_Native> _CreateNativeFromCurrent();

    MhSharedGLContext(std::unique_ptr<_Native> native, void* shareContext);

    std::unique_ptr<_Native> _native;
    void*                    _shareContext = nullptr;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // LIB_MAYAHYDRA_HYDRAEXTENSIONS_MHSHAREDGLCONTEXT_H
