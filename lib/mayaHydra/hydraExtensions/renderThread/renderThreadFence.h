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

#ifndef MAYAHYDRA_RENDER_THREAD_FENCE_H
#define MAYAHYDRA_RENDER_THREAD_FENCE_H

// GPU fence on the AOV: the render thread signals after FramePass::Render, the
// main thread waits before compositing. glFenceSync for GL; a no-op for Vulkan
// and Metal, whose interop orders the composite itself.
//
// GL sync objects are shared across a share group, so either thread can wait
// on the other's fence. Used for read-after-write (CompletedFrame::fence) and
// write-after-read (FrameCopy::compositeDone).

#include <mayaHydraLib/api.h>

#include <cstdint>
#include <memory>

#include <pxr/imaging/hgi/hgi.h>

namespace MAYAHYDRA_NS_DEF {

/// \class RenderThreadFence
///
/// A single frame's GPU fence, created and signalled once on the thread whose
/// context did the work. Immutable after construction, which is what makes it
/// safe to share without a lock.
///
/// For Vulkan the fence is a no-op: each HgiInteropVulkan slot's vkComplete and
/// glComplete semaphores already order the composite after the copy and the
/// next copy into that slot after the composite. Likewise for Metal, where
/// HgiInteropMetal does both on the main thread: it waits for the copy to be
/// scheduled before drawing, and for GL's fence before a slot is reused.
class MAYAHYDRALIB_API RenderThreadFence
{
public:
    /// Create a fence for the work just submitted, and flush. Must be called on
    /// the thread whose context is current and whose commands are being fenced;
    /// the flush belongs here rather than in Wait(), because a fence cannot make
    /// progress until the context that created it has flushed.
    ///
    /// \p hgi When Vulkan or Metal, the fence is a no-op. When GL or null, a GL sync
    /// object is created. Never returns null: with no context current
    /// glFenceSync returns 0 and the fence comes back invalid instead.
    static std::shared_ptr<RenderThreadFence> CreateAndSignal(pxr::Hgi* hgi = nullptr);

    ~RenderThreadFence();

    RenderThreadFence(const RenderThreadFence&)            = delete;
    RenderThreadFence& operator=(const RenderThreadFence&) = delete;

    /// True when Wait() can mean something: a real sync object exists, or this
    /// is a Vulkan or Metal no-op fence.
    bool IsValid() const { return _isNoop || _glFence != nullptr; }

    /// Block the calling thread for up to \p timeoutNanos until this frame's GPU
    /// work completes. Callable from any thread whose context is in the same
    /// share group as the creating context. True only when the GPU work is
    /// known to be complete.
    bool Wait(uint64_t timeoutNanos) const;

    /// Order the CALLING context's subsequent GPU work after this fence without
    /// blocking the CPU. Where Wait() (glClientWaitSync) blocks the calling
    /// thread, this (glWaitSync) queues a wait into the calling context's
    /// command stream and returns immediately.
    ///
    /// Ordering, not completion: the dependent work is ISSUED, not finished, so
    /// a producer that overwrites the source textures on the strength of this
    /// call will tear. False means no sync object and no ordering.
    bool WaitOnGpu() const;

private:
    explicit RenderThreadFence(void* glFence, bool isNoop)
        : _glFence(glFence), _isNoop(isNoop) { }

    // GLsync cast to void* to keep GL headers out of this file. Written once,
    // in CreateAndSignal, before this object is shared.
    void* const _glFence;

    const bool _isNoop;
};

using RenderThreadFencePtr = std::shared_ptr<RenderThreadFence>;

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_FENCE_H
