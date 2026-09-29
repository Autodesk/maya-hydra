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

#ifndef MAYAHYDRALIB_EXT_GPU_BUFFER_BRIDGE_H
#define MAYAHYDRALIB_EXT_GPU_BUFFER_BRIDGE_H

#include <mayaHydraLib/api.h>

#include <pxr/imaging/hgi/externalBuffer.h>
#include <pxr/pxr.h>

#include <cstddef>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

class Hgi;
class HgiExternalBufferArena;

/// \class MhExtGpuBufferBridge
///
/// Turns a VP2 OpenGL vertex buffer into an HgiExternalBuffer the renderer's
/// Hgi can read, hiding which of two very different routes that took.
///
/// \section Routes
///
/// | consumer | route | copies |
/// | -------- | ----- | ------ |
/// | OpenGL | register Maya's buffer with the GL arena | none |
/// | Vulkan | allocate exportable, import into GL, copy into it | one, GPU-side |
///
/// The asymmetry is not a gap in this class, it is the shape of GL/Vulkan
/// interop. Memory sharing between the two exists and the Vulkan route uses
/// it -- an allocation imported through GL_EXT_memory_object is one physical
/// allocation both APIs address -- but it only runs one way, because OpenGL
/// has no entry point that exports an allocation for another API to import,
/// and GL binds memory to a buffer at storage-creation time so an existing
/// VBO cannot be made shareable after the fact.
///
/// Maya allocated its VBO with ordinary GL storage long before a render item
/// reaches us, which puts that allocation permanently out of reach of a
/// Vulkan consumer. So the Vulkan route inverts ownership: Hgi allocates the
/// memory both sides can see, and the remaining copy is GL-to-GL, out of
/// Maya's unshareable buffer into the shared one. It is a GPU-side copy, and
/// still well short of the CPU roundtrip the fallback pays.
///
/// \section Lifetime
///
/// A bridge is a value: it holds a raw arena pointer that its Hgi owns for
/// its own lifetime, so constructing one per publish costs a get-or-create
/// lookup and nothing else. The GL objects backing the Vulkan route hang off
/// the HgiExternalBuffer's keepalive instead of living here, which is what
/// lets this stay copyable and stateless.
///
class MhExtGpuBufferBridge
{
public:
    /// The bridge \p hgi can use, or an invalid one when VP2's buffers cannot
    /// reach it at all -- a VP2 that is not on OpenGL, an Hgi backend with no
    /// external buffer arena, or a driver missing the interop extensions.
    ///
    /// That invalid result is the whole negotiation, settled once per publish
    /// rather than rediscovered per buffer, and the caller reads it as "take
    /// the CPU primvar path".
    MAYAHYDRALIB_API
    static MhExtGpuBufferBridge ForHgi(Hgi *hgi);

    MhExtGpuBufferBridge() = default;

    /// Whether buffers can be shared at all; see ForHgi.
    explicit operator bool() const { return _kind != _Kind::None; }

    /// Whether publishing aliases Maya's buffer rather than copying out of
    /// it. False means the contents have to be republished whenever Maya
    /// rewrites them, even though nothing about the layout changed.
    bool IsZeroCopy() const { return _kind == _Kind::OpenGL; }

    /// For diagnostics: "OpenGL", "Vulkan" or "none".
    MAYAHYDRALIB_API
    const char *Describe() const;

    /// Produce a buffer the consumer can read holding \p mayaGlBuffer's
    /// contents. \p byteSize must cover the whole stream, offset included;
    /// \p copyByteSize is the exact span to read out of Maya's buffer, which
    /// is the smaller of the two on an interleaved stream and the only one
    /// safe to copy. Returns null when the buffer cannot be shared, in which
    /// case the caller falls back to the CPU primvar.
    ///
    /// A copying bridge allocates here, so call it only when the existing
    /// buffer will not do -- see Refresh.
    MAYAHYDRALIB_API
    HgiExternalBufferSharedPtr Create(
        uint32_t mayaGlBuffer,
        size_t byteSize,
        size_t copyByteSize,
        const char *debugName) const;

    /// Bring \p buffer back in step with \p mayaGlBuffer, reusing the
    /// allocation. A no-op on a zero-copy bridge, where the consumer is
    /// already looking at Maya's memory. Returns false if the refresh failed
    /// and the stream should drop to the CPU path.
    ///
    /// \p mayaGlBuffer need not be the buffer Create saw: a copying bridge
    /// owns what it published and only reads Maya's, so VP2 recycling a
    /// buffer between frames is just a change of source here. Reusing this
    /// rather than reallocating is what keeps a deforming object to one
    /// allocation for its lifetime.
    MAYAHYDRALIB_API
    bool Refresh(
        const HgiExternalBufferSharedPtr &buffer,
        uint32_t mayaGlBuffer,
        size_t copyByteSize) const;

    /// Open the producer's frame: wait until the consumer has finished
    /// reading what was published last frame, so this frame's copies may
    /// overwrite it. Call once before publishing anything, on the thread that
    /// owns Maya's GL context.
    ///
    /// A no-op for a zero-copy bridge, which overwrites nothing, and on the
    /// first frame, when nothing has been published yet.
    MAYAHYDRALIB_API
    static void BeginProducerFrame();

    /// Close the producer's frame: tell the consumer that everything published
    /// this frame is written and may be read. Call once after publishing every
    /// render item and, critically, before the consumer's frame begins -- the
    /// arena picks up a publish only in a frame that starts after it.
    MAYAHYDRALIB_API
    static void EndProducerFrame();

    /// Release every GL object the bridge owns.
    ///
    /// Must be called while Maya's GL context is current and before the
    /// renderer is torn down; the imported semaphores cannot be left to
    /// static destruction, which runs after both the context and Tf's
    /// diagnostic delegates are gone.
    MAYAHYDRALIB_API
    static void Shutdown();

private:
    enum class _Kind
    {
        None,
        OpenGL,
        Vulkan,
    };

    _Kind _kind = _Kind::None;
    HgiExternalBufferArena *_arena = nullptr;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_EXT_GPU_BUFFER_BRIDGE_H
