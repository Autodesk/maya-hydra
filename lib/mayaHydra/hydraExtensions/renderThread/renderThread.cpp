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

#include "renderThread.h"

#include "renderThreadStats.h"

#include <mayaHydraLib/debugCodes.h>
#include <mayaHydraLib/profilingUtils.h>

// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hgiGL/texture.h>
#if defined(USD_HAS_HGI_VULKAN)
#include <pxr/imaging/hgiVulkan/blitCmds.h>
#include <pxr/imaging/hgiVulkan/hgi.h>
#endif

#if defined(__APPLE__)
// What @autoreleasepool compiles to, callable from C++.
extern "C" void* objc_autoreleasePoolPush(void);
extern "C" void  objc_autoreleasePoolPop(void* pool);
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF
{

namespace {

/// Drains, on scope exit, the Objective-C objects autoreleased on this thread
/// in its scope. Metal autoreleases objects it hands out, and nothing else
/// drains them on a thread without a run loop, so each would leak.
class ScopedAutoreleasePool
{
public:
#if defined(__APPLE__)
    ScopedAutoreleasePool()
        : _pool(objc_autoreleasePoolPush())
    {
    }
    ~ScopedAutoreleasePool() { objc_autoreleasePoolPop(_pool); }
#else
    // User-provided, so a scoped instance is not reported as unused.
    ScopedAutoreleasePool() { }
#endif

    ScopedAutoreleasePool(const ScopedAutoreleasePool&)            = delete;
    ScopedAutoreleasePool& operator=(const ScopedAutoreleasePool&) = delete;

#if defined(__APPLE__)
private:
    void* _pool;
#endif
};

// The GPU fence budget is long enough that a heavy frame lands, and short
// enough that a wedged GPU costs a visible hitch rather than a hung Maya.
constexpr uint64_t kQuiescenceFenceBudgetNanos = 250ull * 1000ull * 1000ull; // 250ms

// How long the main thread waits for the render thread to leave the render
// index before counting the wait as slow. Never a timeout: the first render of
// a scene, or one that compiles shaders, can take seconds, and pushing into a
// render index that is inside SyncAll is not a recoverable race.
constexpr auto kIdleInterlockWarnAfter = std::chrono::milliseconds(500);

} // namespace

RenderThread::RenderThread() = default;

RenderThread::~RenderThread()
{
    Stop();
}

bool
RenderThread::Start(RenderCallback renderCallback,
                    std::unique_ptr<RenderThreadContext> context,
                    Hgi* hgi,
                    RenderThreadPresentBridge* presentBridge)
{
    if (_thread.joinable())
    {
        TF_CODING_ERROR("RenderThread::Start: thread already running");
        return false;
    }
    _renderCallback = std::move(renderCallback);
    _context = std::move(context);
    _hgi = hgi;
    _presentBridge = presentBridge;
    _mainThread = std::this_thread::get_id();
    _stop.store(false, std::memory_order_release);

    {
        std::lock_guard<std::mutex> lock(_contextMutex);
        _contextState = ContextState::Pending;
    }

    _thread = std::thread(&RenderThread::_ThreadLoop, this);

    // MakeCurrent has to happen on the render thread, so this handshake is the
    // only way the caller can learn whether it succeeded before submitting work.
    ContextState state;
    {
        std::unique_lock<std::mutex> lock(_contextMutex);
        _contextCv.wait(lock, [this] { return _contextState != ContextState::Pending; });
        state = _contextState;
    }

    if (state == ContextState::Failed)
    {
        // Take the thread back down. Left running, its every GL call silently
        // no-ops and the failure surfaces as a blank viewport far from here.
        TF_WARN("RenderThread::Start: the graphics context could not be made "
                "current on the render thread; falling back to synchronous "
                "rendering. Reason: %s",
                _context ? _context->LastError().c_str() : "no context was supplied");
        _stop.store(true, std::memory_order_release);
        _requestCv.notify_all();
        _thread.join();
        _renderCallback = nullptr;
        _context.reset();
        _hgi = nullptr;
        _presentBridge = nullptr;
        RenderThreadStats::instance().SetStarted(false);
        return false;
    }

    RenderThreadStats::instance().SetStarted(true);
    return true;
}

void
RenderThread::Stop()
{
    if (!_thread.joinable())
    {
        return;
    }

    // Teardown order: stop accepting snapshots, join the thread, then free GPU
    // resources on the owning context. Set under the lock the thread waits
    // with, or the notify can land between its check and its sleep.
    {
        std::lock_guard<std::mutex> lock(_requestMutex);
        _stop.store(true, std::memory_order_release);
    }
    _requestCv.notify_all();
    _thread.join();

    _handoff.Clear();
    _lastComposited = CompletedFrame{};
    _presentBridge = nullptr;

    // The fences and the GL context were already torn down at the end of
    // _ThreadLoop, on the thread that owned the context. This destroys the
    // drawing surface, here on the thread that created it.
    _context.reset();

    _renderCallback = nullptr;
    _hasPending.store(false, std::memory_order_release);
}

void
RenderThread::SubmitRequest(RenderThreadRequest request)
{
    {
        std::unique_lock<std::mutex> lock(_requestMutex);
        // A request is never replaced: its scene push already closed a GPU
        // buffer sharing producer frame, and under Vulkan that signalled a
        // binary semaphore only this request's render will wait on. Dropping
        // it would signal the semaphore twice for one wait. WaitForIdle runs
        // before every push, so the slot is normally empty by now.
        _idleCv.wait(lock, [this] {
            return !_hasPending.load(std::memory_order_acquire)
                || _stop.load(std::memory_order_acquire);
        });
        _pendingRequest = std::move(request);
        _hasPending.store(true, std::memory_order_release);
    }
    _requestCv.notify_one();
}

void
RenderThread::_ThreadLoop()
{
    // The context's whole lifetime after creation belongs to this thread, so
    // nothing GPU-side may outlive this function. A null context is not a
    // failure -- it is the "no GPU work" configuration a test uses -- but one
    // that will not come current is, and Start() is waiting to hear about it.
    bool contextOk = true;
    if (_context)
    {
        contextOk = _context->MakeCurrent();
    }
    {
        std::lock_guard<std::mutex> lock(_contextMutex);
        _contextState = contextOk ? ContextState::Ready : ContextState::Failed;
    }
    _contextCv.notify_all();

    if (!contextOk)
    {
        // Start() will join us. Do not touch the context further: it is not
        // current, so any GL call would silently no-op.
        return;
    }

    // After MakeCurrent, because the arena's framebuffers belong to whichever
    // context is current when they are created.
    _AcquireContextArena();

    while (true)
    {
        RenderThreadRequest request;
        bool hasRequest = false;
        const std::function<void()>* job = nullptr;
        {
            std::unique_lock<std::mutex> lock(_requestMutex);
            _requestCv.wait(lock, [this] {
                return _stop.load(std::memory_order_acquire)
                    || _hasPending.load(std::memory_order_acquire)
                    || _pendingJob;
            });

            // A pending request is rendered even when stopping, for the same
            // reason SubmitRequest never replaces one.
            if (_stop.load(std::memory_order_acquire)
                && !_hasPending.load(std::memory_order_acquire) && !_pendingJob)
            {
                break;
            }

            if (_pendingJob)
            {
                job = _pendingJob;
                _inFlight.store(true, std::memory_order_release);
            }
            else if (_hasPending.load(std::memory_order_acquire))
            {
                request = std::move(_pendingRequest);
                _hasPending.store(false, std::memory_order_release);
                hasRequest = true;

                // Set under the same lock that clears _hasPending, so a drain
                // can never observe both flags false with a request still to
                // be rendered.
                _inFlight.store(true, std::memory_order_release);
            }
        }

        if (hasRequest)
        {
            // Wakes a SubmitRequest waiting for the pending slot to empty.
            _idleCv.notify_all();

            {
                ScopedAutoreleasePool pool;
                RenderThreadStats::instance().SetRendering(true, _mainThread);
                _ExecuteRequest(request);
                RenderThreadStats::instance().SetRendering(false, _mainThread);
            }

            {
                std::lock_guard<std::mutex> lock(_requestMutex);
                _inFlight.store(false, std::memory_order_release);
            }
            _idleCv.notify_all();
        }
        else if (job)
        {
            {
                ScopedAutoreleasePool pool;
                RenderThreadStats::instance().SetRendering(true, _mainThread);
                _WithContextArena(*job);
                RenderThreadStats::instance().SetRendering(false, _mainThread);
            }

            {
                std::lock_guard<std::mutex> lock(_requestMutex);
                _pendingJob = nullptr;
                _inFlight.store(false, std::memory_order_release);
            }
            _idleCv.notify_all();
        }
    }

    // Teardown on the owning thread: the fences' sync objects and the arena's
    // framebuffers were created on this context, so they have to go before it
    // does. Clearing the handoff drops the last reference to any published
    // frame's fence, which is what deletes those sync objects here.
    ScopedAutoreleasePool teardownPool;
    _handoff.Clear();
    _lastComposited = CompletedFrame{};
    _RecordRenderDone(nullptr);
    _ReleaseCopyTargets();
    _ReleaseContextArena();
    if (_context)
    {
        _context->ReleaseCurrent();
        _context->DestroyContext();
    }
}

void
RenderThread::_AcquireContextArena()
{
    // Vulkan and Metal have no arena concept, and a null Hgi is the
    // configuration a non-GPU test uses.
    HgiGL* hgiGl = dynamic_cast<HgiGL*>(_hgi);
    if (hgiGl == nullptr)
    {
        return;
    }

    _contextArena = hgiGl->CreateContextArena();
    if (!_contextArena)
    {
        // The render still runs, but on Hgi's default arena, whose framebuffers
        // were made against Maya's context, so the symptom is wrong or missing
        // pixels rather than an error and this warning is the only clue.
        TF_WARN("RenderThread: HgiGL::CreateContextArena failed, so the render "
                "thread will share Maya's default framebuffer arena. Rendering "
                "off-thread in this state is not expected to be correct.");
    }
}

namespace {

// Make \p dst a texture matching \p src, reusing it when it already does.
// Returns false if there is nothing to match or the allocation failed.
bool
EnsureMatchingTexture(Hgi* hgi, const HgiTextureHandle& src, HgiTextureHandle& dst)
{
    if (!src)
    {
        // A pass without a depth AOV hands back an empty handle, so an empty
        // source is not an error.
        if (dst)
        {
            hgi->DestroyTexture(&dst);
        }
        return true;
    }

    const HgiTextureDesc& srcDesc = src->GetDescriptor();
    if (dst)
    {
        const HgiTextureDesc& dstDesc = dst->GetDescriptor();
        if (dstDesc.dimensions == srcDesc.dimensions && dstDesc.format == srcDesc.format)
        {
            return true;
        }
        // Resized or reformatted. The old one is this thread's, so it goes here.
        hgi->DestroyTexture(&dst);
    }

    HgiTextureDesc desc = srcDesc;
    desc.debugName = "MayaHydra render thread AOV copy";
    // This is a blit target, so the source's initial-data pointer must not be
    // carried into a new allocation.
    desc.initialData = nullptr;
    desc.pixelsByteSize = 0;
    // Both ends of a GPU-to-GPU copy have to be usable as such, and the main
    // thread additionally reads this one through HgiInterop.
    desc.usage = srcDesc.usage | HgiTextureUsageBitsShaderRead;

    dst = hgi->CreateTexture(desc);
    return bool(dst);
}

// Copy \p src into \p dst on the GPU. Both must be HgiGL textures of matching
// dimensions and format, which EnsureMatchingTexture guarantees.
bool
CopyGlTexture(const HgiTextureHandle& src, const HgiTextureHandle& dst)
{
    const HgiGLTexture* srcGl = dynamic_cast<const HgiGLTexture*>(src.Get());
    const HgiGLTexture* dstGl = dynamic_cast<const HgiGLTexture*>(dst.Get());
    if (!srcGl || !dstGl)
    {
        TF_WARN("RenderThread: the AOV is not an HgiGL texture, so it cannot be "
                "copied into render-thread-owned storage.");
        return false;
    }

    const GfVec3i& dims = src->GetDescriptor().dimensions;
    glCopyImageSubData(
        srcGl->GetTextureId(), GL_TEXTURE_2D, 0, 0, 0, 0,
        dstGl->GetTextureId(), GL_TEXTURE_2D, 0, 0, 0, 0,
        dims[0], dims[1], 1);
    return true;
}

#if defined(USD_HAS_HGI_VULKAN)
// Copy \p src into \p dst on the GPU under Vulkan. Submitted with NoWait, so
// the frame's fence covers the blit as it does the GL path's copy.
bool
CopyVulkanTexture(Hgi* hgi, const HgiTextureHandle& src, const HgiTextureHandle& dst)
{
    HgiVulkan* hgiVulkan = dynamic_cast<HgiVulkan*>(hgi);
    if (!hgiVulkan)
    {
        TF_WARN("RenderThread: the Hgi reports Vulkan but is not an HgiVulkan, "
                "so the AOV copy cannot run.");
        return false;
    }

    HgiBlitCmdsUniquePtr blitCmds = hgiVulkan->CreateBlitCmds();
    HgiVulkanBlitCmds* vkBlitCmds = dynamic_cast<HgiVulkanBlitCmds*>(blitCmds.get());
    if (!vkBlitCmds)
    {
        TF_WARN("RenderThread: could not get HgiVulkanBlitCmds for the AOV copy.");
        return false;
    }

    vkBlitCmds->BlitTexture(src, dst);
    hgiVulkan->SubmitCmds(blitCmds.get(), HgiSubmitWaitTypeNoWait);
    return true;
}
#endif

// Hgi has no backend-neutral texture-to-texture GPU copy, so the copy branches
// on the API. A backend that is neither GL nor Vulkan needs its own case here.
// Metal has none: its frames are copied by the present bridge, and one the
// bridge could not prepare is dropped here.
bool
CopyTexture(Hgi* hgi, const HgiTextureHandle& src, const HgiTextureHandle& dst)
{
    const TfToken& api = hgi->GetAPIName();
    if (api == HgiTokens->OpenGL)
    {
        return CopyGlTexture(src, dst);
    }
#if defined(USD_HAS_HGI_VULKAN)
    if (api == HgiTokens->Vulkan)
    {
        return CopyVulkanTexture(hgi, src, dst);
    }
#endif

    TF_WARN("RenderThread: the AOV copy is not implemented for the %s Hgi, so "
            "this frame is dropped rather than published as a handle to the "
            "live AOV.", api.GetText());
    return false;
}

} // namespace

bool
RenderThread::_CopyIntoOwnedTextures(const RenderResult& result, FrameCopy& out)
{
    Hgi* hgi = result.srcHgi ? result.srcHgi : _hgi;
    if (!hgi || !result.srcColor)
    {
        // No colour means the render produced nothing to hand over. The caller
        // counts it; the render callback is the place that knows why.
        return false;
    }

    if (!EnsureMatchingTexture(hgi, result.srcColor, out.color)
        || !EnsureMatchingTexture(hgi, result.srcDepth, out.depth))
    {
        TF_WARN("RenderThread: could not allocate an AOV copy target, so this "
                "frame is dropped rather than published as a handle to the "
                "live AOV.");
        return false;
    }

    auto copyFn = [&](const HgiTextureHandle& src, HgiTextureHandle& dst) -> bool {
        if (!src) return true;
        return CopyTexture(hgi, src, dst);
    };
    if (!copyFn(result.srcColor, out.color))
    {
        return false;
    }
    if (!copyFn(result.srcDepth, out.depth))
    {
        return false;
    }

    // Issued on this thread's context after the render that produced the
    // source, so submission order alone puts the copy behind it. The frame's
    // fence is created after this returns and so covers the copy too.
    return true;
}

void
RenderThread::_ReleaseCopyTargets()
{
    // Fences first, and unconditionally: they are GL sync objects, so they go
    // while this thread's context is still current, whether or not an Hgi
    // remains to destroy textures with.
    {
        std::lock_guard<std::mutex> lock(_copyFenceMutex);
        for (FrameCopy& copy : _copyTargets)
        {
            copy.compositeDone.reset();
        }
    }

    if (!_hgi)
    {
        return;
    }
    for (FrameCopy& copy : _copyTargets)
    {
        if (copy.color)
        {
            _hgi->DestroyTexture(&copy.color);
        }
        if (copy.depth)
        {
            _hgi->DestroyTexture(&copy.depth);
        }
    }
}

void
RenderThread::_ReleaseContextArena()
{
    if (!_contextArena)
    {
        return;
    }

    HgiGL* hgiGl = dynamic_cast<HgiGL*>(_hgi);
    if (hgiGl == nullptr)
    {
        return;
    }

    // HgiGL requires the arena be unset before it is destroyed.
    hgiGl->SetContextArena(HgiGLContextArenaHandle());
    hgiGl->DestroyContextArena(&_contextArena);
}

void
RenderThread::_WithContextArena(const std::function<void()>& fn)
{
    // Select this thread's arena, so Hydra's framebuffers are created in and
    // looked up from this context. Scoped rather than set once, because the
    // arena is state on the shared Hgi instance.
    //
    // Safe against the concurrent composite only because the composite reads
    // the AOV's GL id through HgiInterop rather than submitting Hgi commands.
    // If that changes, the arenas have to become per thread, not per render.
    HgiGL* hgiGl = dynamic_cast<HgiGL*>(_hgi);
    const bool useArena = (hgiGl != nullptr) && bool(_contextArena);
    if (useArena)
    {
        hgiGl->SetContextArena(_contextArena);
    }

    fn();

    if (useArena)
    {
        hgiGl->SetContextArena(HgiGLContextArenaHandle());
    }
}

void
RenderThread::_ExecuteRequest(const RenderThreadRequest& request)
{
    if (!_renderCallback)
    {
        return;
    }

    // The callback, supplied by the render override, runs FramePass::Render
    // (Hydra's SyncAll+Execute+SubmitCmds) and returns the AOV texture handles.
    MH_PROFILE_SCOPE("RenderThread_ExecuteRequest");

    // Read-after-write: the scene push may have copied into shared buffers on
    // Maya's context, and a share group does not order that context's commands
    // against this one's.
    if (request.producerFence && !request.producerFence->WaitOnGpu())
    {
        RenderThreadStats::instance().RecordSharedBufferFenceFailure();
        TF_WARN("RenderThread: frame %llu's scene push left no usable GPU "
                "fence, so this render may read shared buffers before Maya's "
                "copies into them land.",
                static_cast<unsigned long long>(request.frameId));
    }

    RenderResult result;
    _WithContextArena([&] { result = _renderCallback(request); });

    // Take a copy of the AOV before anything else can render into it. Ordered
    // before the fence deliberately: the copy is what gets published, so the
    // fence has to cover it and not just the render.
    HgiTextureHandle publishColor = result.srcColor;
    HgiTextureHandle publishDepth = result.srcDepth;
    int              publishCopyIndex = -1;

    // Under Vulkan and Metal the present bridge's copy into GL-readable
    // storage is the owned copy, with its own ordering against the
    // composite, so the copy pairs below are not used.
    bool prepared = false;
#if defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
    HgiInteropFrame interopFrame;
    if (_presentBridge && result.srcColor)
    {
        MH_PROFILE_SCOPE("RenderThread_PrepareTransfer");
        interopFrame = _presentBridge->Prepare(
            result.srcHgi ? result.srcHgi : _hgi, result.srcColor, result.srcDepth);
        prepared = bool(interopFrame);
    }
#endif

    if (!prepared)
    {
        MH_PROFILE_SCOPE("RenderThread_AOVCopy");

        const int  targetIndex = _nextCopyTarget;
        FrameCopy& target      = _copyTargets[targetIndex];

        // Write-after-read: do not overwrite this pair until the composite that
        // read it has finished. The main thread's blit is only ISSUED by the
        // time CompositeLatest returns, so nothing else orders our copy after a
        // blit on a different context.
        RenderThreadFencePtr priorComposite;
        {
            std::lock_guard<std::mutex> lock(_copyFenceMutex);
            priorComposite = target.compositeDone;

            // Cleared under the same lock that took it, so the next pass does
            // not wait on a stale, already-signalled fence.
            target.compositeDone.reset();
        }
        // Only valid fences are stored, so the wait cannot fail.
        if (priorComposite)
        {
            priorComposite->WaitOnGpu();
        }

        if (!_CopyIntoOwnedTextures(result, target))
        {
            // Not published: the alternative, handing over the live AOV, is
            // the tearing case. The render still read shared buffers, so the
            // next scene push still needs a fence to order its writes after.
            RenderThreadStats::instance().RecordAovCopyFailure();
            _RecordRenderDone(RenderThreadFence::CreateAndSignal(_hgi));
            return;
        }
        _nextCopyTarget  = (_nextCopyTarget + 1) % kNumCopyTargets;
        publishColor     = target.color;
        publishDepth     = target.depth;
        publishCopyIndex = targetIndex;
    }

    // One fence per frame, created on the context that just did the work and
    // carried with the frame so the consumer waits for THIS frame.
    MH_PROFILE_SCOPE("RenderThread_FenceAndPublish");
    RenderThreadFencePtr fence = RenderThreadFence::CreateAndSignal(_hgi);
    if (!fence->IsValid())
    {
        RenderThreadStats::instance().RecordFenceSignalFailure();
    }
    _RecordRenderDone(fence);

    // After the callback returned, so this counts frames rendered rather than
    // requests accepted.
    RenderThreadStats::instance().RecordFrameRenderedOffThread();

    CompletedFrame completed;
    completed.frameId   = request.frameId;
    completed.srcColor  = publishColor;
    completed.srcDepth  = publishDepth;
    completed.srcHgi    = result.srcHgi;
    completed.dstRegion = request.dstRegion;
    completed.fence     = std::move(fence);
    completed.copyIndex = publishCopyIndex;
#if defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
    completed.interopFrame = interopFrame;
#endif

    _handoff.PublishCompletedFrame(completed);
}

void
RenderThread::_RecordRenderDone(RenderThreadFencePtr fence)
{
    std::lock_guard<std::mutex> lock(_lastRenderDoneMutex);
    _lastRenderDone = std::move(fence);
}

bool
RenderThread::OrderAfterLastRender()
{
    RenderThreadFencePtr lastRender;
    {
        std::lock_guard<std::mutex> lock(_lastRenderDoneMutex);
        lastRender = _lastRenderDone;
    }
    // Nothing rendered yet, so nothing to order after.
    if (!lastRender)
    {
        return true;
    }
    if (!lastRender->WaitOnGpu())
    {
        RenderThreadStats::instance().RecordSharedBufferFenceFailure();
        return false;
    }
    return true;
}

bool
RenderThread::CompositeLatest(
    RenderThreadPresentBridge& presentBridge,
    VtValue const&            dstFramebuffer,
    std::chrono::milliseconds timeout)
{
    // Runs on the main thread at the start of the render op, compositing the
    // latest completed AOV into Maya's FBO and accepting one frame of lag.
    MH_PROFILE_SCOPE("RenderThread_CompositeLatest");

    CompletedFrame frame;
    {
        MH_PROFILE_SCOPE("RenderThread_HandoffWait");
        frame = _handoff.WaitForCompletedFrame(timeout);
    }
    if (!frame.valid)
    {
        // Nothing published yet, or the handoff was just cleared by
        // DiscardInFlight, so there is no frame that may be shown.
        return false;
    }

    // Order the blit behind THIS frame's fence, on the GPU. The fence travels
    // with the frame, so it covers the AOV handles about to be read.
    //
    // GPU-side (glWaitSync), not a CPU block (glClientWaitSync), so that frame
    // time is max(CPU, GPU) rather than their sum. A GPU-side wait cannot
    // report "not ready", so false below means no usable fence, not a slow
    // GPU, and the blit is only issued on return -- hence the composite-done
    // fence later.
    bool fenceReady = false;
    {
        MH_PROFILE_SCOPE("RenderThread_FenceWait");
        fenceReady = frame.fence && frame.fence->WaitOnGpu();
    }
    if (!fenceReady)
    {
        RenderThreadStats::instance().RecordFenceWaitTimeout();
        TF_WARN("RenderThread::CompositeLatest: frame %llu has no usable GPU "
                "fence, so the blit cannot be ordered behind the render that "
                "produced it.",
                static_cast<unsigned long long>(frame.frameId));

        // Show the last frame that did complete rather than nothing: Maya has
        // already cleared its FBO by this point, so returning false reads as a
        // grey viewport rather than as a repeated frame.
        if (!_lastComposited.valid)
        {
            return false;
        }
        frame = _lastComposited;
    }

    // Blit both colour AND depth, so main-thread VP2 overlays depth-compose
    // correctly over the one-frame-stale colour.
    {
        MH_PROFILE_SCOPE("RenderThread_Blit");
#if defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
        if (frame.interopFrame)
        {
            presentBridge.CompositePrepared(
                frame.interopFrame, bool(frame.srcDepth), dstFramebuffer, frame.dstRegion);
        }
        else
#endif
        {
            presentBridge.Composite(
                frame.srcHgi,
                frame.srcColor,
                frame.srcDepth,
                dstFramebuffer,
                frame.dstRegion);
        }
    }

    // Hand the pair back to the render thread with a fence saying when this
    // blit is done reading it. Created after Composite, on Maya's context, so
    // it covers the blit just issued; CreateAndSignal's flush is what makes the
    // sync object reachable by the render thread's wait, which would otherwise
    // stay pending until Maya's own end-of-frame flush. copyIndex is -1 for a
    // frame the present bridge prepared, which orders its own reuse.
    if (frame.copyIndex >= 0 && frame.copyIndex < kNumCopyTargets)
    {
        RenderThreadFencePtr compositeDone =
            RenderThreadFence::CreateAndSignal(frame.srcHgi);
        if (compositeDone->IsValid())
        {
            std::lock_guard<std::mutex> lock(_copyFenceMutex);

            // Overwrites rather than accumulates: a pair is read by only one
            // composite between two of its own writes, so a fence already here
            // belongs to a blit the render thread has since passed.
            _copyTargets[frame.copyIndex].compositeDone = std::move(compositeDone);
        }
        else
        {
            // The next write to this pair is now unordered against the blit
            // just issued, so it may tear.
            RenderThreadStats::instance().RecordCompositeFenceSignalFailure();
        }
    }

    _lastComposited = frame;
    RenderThreadStats::instance().RecordFrameComposited();
    return true;
}

void
RenderThread::RunOnRenderThread(const std::function<void()>& job)
{
    if (!_thread.joinable())
    {
        job();
        return;
    }

    MH_PROFILE_SCOPE("RenderThread_RunOnRenderThread");

    {
        std::unique_lock<std::mutex> lock(_requestMutex);
        _idleCv.wait(lock, [this] {
            return !_inFlight.load(std::memory_order_acquire)
                && !_hasPending.load(std::memory_order_acquire) && !_pendingJob;
        });
        _pendingJob = &job;
    }
    _requestCv.notify_one();

    std::unique_lock<std::mutex> lock(_requestMutex);
    _idleCv.wait(lock, [this] { return !_pendingJob; });
}

bool
RenderThread::WaitForIdle()
{
    if (!_thread.joinable())
    {
        return true;
    }

    MH_PROFILE_SCOPE("RenderThread_WaitForIdle");

    auto isIdle = [this] {
        return !_inFlight.load(std::memory_order_acquire)
            && !_hasPending.load(std::memory_order_acquire);
    };

    std::unique_lock<std::mutex> lock(_requestMutex);
    if (_idleCv.wait_for(lock, kIdleInterlockWarnAfter, isIdle))
    {
        return true;
    }

    // Counted, but still waited out: the push this guards would otherwise
    // invalidate the render index during the render thread's SyncAll. Reported
    // only under a debug flag, because the first sync of every heavy scene
    // gets here and is not a fault.
    RenderThreadStats::instance().RecordIdleWaitTimeout();
    const auto slowStart = std::chrono::steady_clock::now();
    lock.unlock();
    TF_DEBUG(MAYAHYDRALIB_RENDER_THREAD)
        .Msg("RenderThread::WaitForIdle: the render thread is still in the "
             "render index after %lld ms; waiting for it to finish.\n",
             static_cast<long long>(kIdleInterlockWarnAfter.count()));
    lock.lock();
    _idleCv.wait(lock, isIdle);
    lock.unlock();

    TF_DEBUG(MAYAHYDRALIB_RENDER_THREAD)
        .Msg("RenderThread::WaitForIdle: the render thread went idle after a "
             "further %lld ms.\n",
             static_cast<long long>(
                 std::chrono::duration_cast<std::chrono::milliseconds>(
                     std::chrono::steady_clock::now() - slowStart)
                     .count()));
    return false;
}

void
RenderThread::DiscardInFlight()
{
    // Nothing produced before this call may reach the screen afterwards.
    RenderThreadStats::instance().RecordDrain();

    // A request the thread has not started is rendered rather than dropped, as
    // SubmitRequest explains, and its frame is discarded with the rest below.
    // A render already running cannot be cancelled either.
    WaitForIdle();

    // Also releases the last reference to those frames' fences, deleting the
    // sync objects.
    _handoff.Clear();
    _lastComposited = CompletedFrame{};
}

bool
RenderThread::WaitForQuiescence()
{
    // A barrier, not a cancellation: the frame in flight is the one the caller
    // wants, so it is waited for and left in the handoff. Without a time limit,
    // like WaitForIdle: giving up would only show the previous frame, and the
    // next render op's WaitForIdle would wait for this one regardless.
    auto isIdle = [this] {
        return !_hasPending.load(std::memory_order_acquire)
            && !_inFlight.load(std::memory_order_acquire);
    };
    {
        std::unique_lock<std::mutex> lock(_requestMutex);
        if (!_idleCv.wait_for(lock, kIdleInterlockWarnAfter, isIdle))
        {
            // Counted, but reported only under a debug flag: a frame that
            // compiles shaders gets here and is not a fault.
            RenderThreadStats::instance().RecordDrainTimeout();
            const auto slowStart = std::chrono::steady_clock::now();
            _idleCv.wait(lock, isIdle);
            TF_DEBUG(MAYAHYDRALIB_RENDER_THREAD)
                .Msg("RenderThread::WaitForQuiescence: the render thread went "
                     "idle after a further %lld ms.\n",
                     static_cast<long long>(
                         std::chrono::duration_cast<std::chrono::milliseconds>(
                             std::chrono::steady_clock::now() - slowStart)
                             .count()));
        }
    }

    // CPU-side idle is not enough for a capture: the frame is published but its
    // GPU writes may still be in flight. Left published, so the next render op
    // composites it.
    const CompletedFrame latest = _handoff.TryGetCompletedFrame();
    if (latest.valid && latest.fence && !latest.fence->Wait(kQuiescenceFenceBudgetNanos))
    {
        RenderThreadStats::instance().RecordFenceWaitTimeout();
        TF_WARN("RenderThread::WaitForQuiescence: the latest frame's GPU work "
                "did not complete within the budget.");
        return false;
    }

    return true;
}

bool CanRunVulkanRenderThread()
{
#if defined(USD_HAS_HGI_VULKAN) && defined(HGIVULKAN_HAS_THREADED_SUBMISSION) \
    && defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
    return true;
#else
    return false;
#endif
}

} // namespace MAYAHYDRA_NS_DEF
