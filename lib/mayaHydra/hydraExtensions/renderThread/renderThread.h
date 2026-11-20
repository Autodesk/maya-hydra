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

#ifndef MAYAHYDRA_RENDER_THREAD_H
#define MAYAHYDRA_RENDER_THREAD_H

// The render thread owns its graphics context and runs HdEngine::Execute while
// overlap is running. hgi/hgi.h requires submission on the "main thread", which
// for OpenGL it defines as the thread the GL context is bound on, so a thread
// owning its own context satisfies that contract. hgiGL/hgi.h then requires a
// context arena per GL context, for the container resources (framebuffers)
// that cannot be shared.
//
// The main thread pushes scene state only while this thread is idle (see
// WaitForIdle), and MayaHydraSceneIndex queues the notices Maya callbacks send
// in between. This thread itself makes no Maya API calls. Hydra's sync on it
// pulls prim data through the scene index chain, where
// RenderThreadCaptureSceneIndex serves a copy taken on the main thread rather
// than adapters that read Maya when pulled; see doc/renderThreadPipelining.md.

#include "renderThreadContext.h"
#include "renderThreadFence.h"
#include "renderThreadHandoff.h"
#include "renderThreadPresentBridge.h"

#include <mayaHydraLib/api.h>

// clang-format off
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(_MSC_VER)
#pragma warning(push)
#endif
// clang-format on

#include <pxr/base/gf/vec4i.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgiGL/hgi.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/task.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace MAYAHYDRA_NS_DEF {

/// One render request handed to the render thread.
struct RenderThreadRequest
{
    /// Increases with every request; names the frame in warnings.
    uint64_t frameId{0};

    /// FramePass::GetRenderTasks, owned by the caller and not modified here.
    pxr::HdTaskSharedPtrVector renderTasks;

    pxr::TfToken colorAovToken;
    pxr::TfToken depthAovToken;

    /// The render region (left, bottom, width, height) for the composite.
    pxr::GfVec4i dstRegion{0, 0, 0, 0};

    /// Passes to render, in order, as indices into the render override's pass
    /// array. Decided on the main thread, because deciding it means reading
    /// Maya (MAnimControl::isPlaying, the visible-pass command state).
    std::vector<int> passIndices;

    /// Signalled on Maya's context after this frame's scene push. The render
    /// thread queues a GPU wait on it before rendering, so its draws read what
    /// the push copied into shared buffers on that other context. Null when
    /// the push wrote nothing the render thread reads.
    RenderThreadFencePtr producerFence;
};

/// \class RenderThread
///
/// The main thread submits requests via SubmitRequest() and composites the latest
/// completed frame via CompositeLatest(), accepting one frame of lag.
class MAYAHYDRALIB_API RenderThread
{
public:
    struct RenderResult
    {
        pxr::HgiTextureHandle srcColor;
        pxr::HgiTextureHandle srcDepth;
        pxr::Hgi*             srcHgi{nullptr};
    };
    using RenderCallback = std::function<RenderResult(const RenderThreadRequest&)>;

    RenderThread();
    ~RenderThread();

    RenderThread(const RenderThread&)            = delete;
    RenderThread& operator=(const RenderThread&) = delete;

    /// Must be called before SubmitRequest(). \p context is created on the main
    /// thread but made current, released and destroyed on the render thread;
    /// \p hgi supplies this thread's HgiGL arena and is ignored for non-GL
    /// backends. Both may be null. Returns false, having rejoined the thread,
    /// when the context could not be made current. \p presentBridge, when
    /// given, prepares each frame on this thread (see
    /// RenderThreadPresentBridge::Prepare) and must outlive Stop().
    bool Start(RenderCallback renderCallback,
               std::unique_ptr<RenderThreadContext> context = nullptr,
               pxr::Hgi* hgi = nullptr,
               RenderThreadPresentBridge* presentBridge = nullptr);

    /// Drains pending requests and joins the thread. GPU resources must be
    /// freed on the owning context after this returns.
    void Stop();

    /// Depth-1, never replaced: if a request is still pending, this blocks
    /// until the render thread picks it up. Every request's scene push has
    /// already closed a GPU buffer sharing producer frame, which under Vulkan
    /// signalled a semaphore that only that request's render waits on.
    void SubmitRequest(RenderThreadRequest request);

    /// Composite the latest completed frame into Maya's FBO, on the main thread
    /// at the start of the render op. \p timeout covers only the wait for a
    /// frame in the handoff; the GPU fence has its own budget (kFenceBudget),
    /// and a frame whose fence is not ready falls back to re-compositing the
    /// last frame that was, because Maya has already cleared its FBO.
    bool CompositeLatest(
        RenderThreadPresentBridge& presentBridge,
        pxr::VtValue const&        dstFramebuffer,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(16));

    /// For pick, resize and a change of panel: wait for pending and running
    /// renders to finish, then clear the handoff so no frame produced before
    /// this call can be composited after it. Pending work is rendered rather
    /// than dropped, for the reason given on SubmitRequest. The wait has no
    /// time limit, because the caller goes on to render on the main thread or
    /// for another panel, and must not overlap a render still running.
    void DiscardInFlight();

    /// For an image capture, or a render op compositing its own frame: wait for
    /// the pending request to be rendered, published and its GPU work completed,
    /// and keep it in the handoff so it can be composited. Returns false on
    /// timeout.
    bool WaitForQuiescence();

    /// Wait until the render thread is not touching the render index. The main
    /// thread must hold this before pushing scene state, because
    /// MayaHydraSceneIndex::FlushPendingUpdates creates adapters and emits
    /// dirty notices against the same HdRenderIndex the render thread is inside
    /// HdEngine::Execute on, and nothing else serialises the two. Always waits
    /// until idle; returns false if that took long enough to be reported.
    bool WaitForIdle();

    /// Order the calling context's subsequent GPU work after the last render's
    /// GPU work, without blocking the CPU. The main thread calls this past
    /// WaitForIdle and before overwriting anything that render read, such as
    /// copied GPU shared buffers. Returns false if the last render left no
    /// usable fence.
    bool OrderAfterLastRender();

    /// Run \p job on the render thread, with its context and HgiGL arena
    /// current, and return once it has run. Waits first for pending and
    /// running renders to finish. The caller must keep Maya data out of the
    /// job's reach, as for a render. Runs \p job on the calling thread if the
    /// render thread is not running.
    void RunOnRenderThread(const std::function<void()>& job);

    bool IsRunning() const { return _thread.joinable(); }

private:
    void _ThreadLoop();

    void _ExecuteRequest(const RenderThreadRequest& request);

    /// Runs \p fn with this thread's HgiGL arena selected, when there is one.
    void _WithContextArena(const std::function<void()>& fn);

    /// Publish \p fence, created after a render's GPU work, to
    /// OrderAfterLastRender.
    void _RecordRenderDone(RenderThreadFencePtr fence);

    /// Both run on the render thread with its context current; an arena's
    /// framebuffers belong to that context.
    void _AcquireContextArena();
    void _ReleaseContextArena();

    /// A render-thread-owned copy of the AOV as it looked when a frame finished.
    struct FrameCopy
    {
        pxr::HgiTextureHandle color;
        pxr::HgiTextureHandle depth;

        /// Signalled by the main thread after the blit that READ this pair,
        /// waited on by the render thread before it rewrites the pair: the
        /// composite only ISSUES its blit. Null until first composited.
        RenderThreadFencePtr compositeDone;
    };

    /// FramePass::GetRenderTexture returns one texture per token, reused by
    /// every frame, so publishing that handle would let the render thread begin
    /// frame N+1 into the texture being composited for frame N. Returns false
    /// if the copy failed; the frame is then not published.
    bool _CopyIntoOwnedTextures(const RenderResult& result, FrameCopy& out);

    /// Runs on the render thread before its context goes.
    void _ReleaseCopyTargets();

    /// Context bring-up handshake: MakeCurrent can only run on the render
    /// thread, so the thread reports back and Start() waits for the answer.
    enum class ContextState
    {
        Pending,
        Ready,
        Failed
    };

    RenderCallback                 _renderCallback;
    std::unique_ptr<RenderThreadContext> _context;

    /// A null arena means either no Hgi was supplied or the backend is not GL,
    /// in which case the render runs on whatever arena Hgi defaults to.
    pxr::Hgi*                      _hgi{nullptr};
    pxr::HgiGLContextArenaHandle   _contextArena;
    RenderThreadPresentBridge*     _presentBridge{nullptr};
    std::mutex                     _contextMutex;
    std::condition_variable        _contextCv;
    ContextState                   _contextState{ContextState::Pending};
    std::thread                    _thread;
    std::thread::id                _mainThread;
    mutable std::mutex             _requestMutex;
    std::condition_variable        _requestCv;
    RenderThreadRequest            _pendingRequest;
    std::atomic<bool>              _hasPending{false};
    std::atomic<bool>              _stop{false};

    /// True from when the render thread takes a request until it has published
    /// the result. Guarded by _requestMutex on both edges; this is what makes a
    /// drain wait for completion, since _hasPending goes false at pickup.
    std::atomic<bool>              _inFlight{false};

    /// Set by RunOnRenderThread, and cleared by the render thread once the job
    /// has run. Guarded by _requestMutex.
    const std::function<void()>*   _pendingJob{nullptr};

    /// Signalled when the thread goes idle, so a drain need not poll.
    std::condition_variable        _idleCv;

    /// Owned pairs, used in rotation. Three rather than two, which would make
    /// producer and consumer take strict turns. A throughput choice;
    /// correctness comes from FrameCopy::compositeDone.
    static constexpr int kNumCopyTargets = 3;
    FrameCopy         _copyTargets[kNumCopyTargets];
    int               _nextCopyTarget{0};

    /// Guards the compositeDone fence slots in _copyTargets, the only members
    /// of those structs written by BOTH threads. Not _requestMutex: holding
    /// that across a composite would couple the present to the handshake.
    std::mutex        _copyFenceMutex;

    /// Signalled after the last render's GPU work; see OrderAfterLastRender.
    /// Written by the render thread, read by the main thread.
    RenderThreadFencePtr _lastRenderDone;
    std::mutex           _lastRenderDoneMutex;

    /// The last frame actually composited, standing in for a frame whose fence
    /// is not ready. Holds a reference to that frame's copy pair, which stops
    /// the rotation overwriting it.
    CompletedFrame    _lastComposited;

    /// Each frame carries its own fence through the handoff.
    RenderThreadHandoff            _handoff;
};

/// Whether this USD lets a render thread drive HgiVulkan: its Vulkan backend
/// must accept submissions from several threads, and HgiInterop must prepare
/// frames on the render thread for the main thread to composite.
MAYAHYDRALIB_API bool CanRunVulkanRenderThread();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_H
