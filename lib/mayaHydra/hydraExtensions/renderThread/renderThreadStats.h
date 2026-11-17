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

#ifndef MAYAHYDRA_RENDER_THREAD_STATS_H
#define MAYAHYDRA_RENDER_THREAD_STATS_H

// Observability for the render thread.
//
// Every overlap site is guarded by "flag on AND thread started", so when the
// context cannot come current the guard falls back to synchronous rendering.
// A test that enables MAYAHYDRA_PIPELINE_OVERLAP and only checks that the
// viewport still renders passes on that fallback path, and keeps passing after
// the render thread breaks. The counters below are the assertions instead:
// thread started, own arena taken, N frames rendered off thread and composited.

#include <mayaHydraLib/api.h>

#include <atomic>
#include <cstdint>

namespace MAYAHYDRA_NS_DEF {

/// A snapshot of the render-thread counters, read on the main thread.
struct RenderThreadStatsSnapshot
{
    /// The overlap flag is on; `started` says whether it took effect.
    bool overlapEnabled = false;

    /// Thread running with its context current, not the silent fallback.
    bool started = false;

    /// The thread obtained its own HgiGL context arena. False while `started`
    /// is true means the render ran on Hgi's default arena, whose framebuffers
    /// belong to Maya's context, so the pixels are not trustworthy.
    bool contextArenaAcquired = false;

    /// The thread owns its drawing surface rather than borrowing Maya's.
    bool ownsSurface = false;

    uint64_t framesRenderedOffThread = 0;

    /// Frames the main thread composited from the handoff.
    uint64_t framesComposited = 0;

    /// Frames the main thread rendered itself while the thread ran: first-frame
    /// prime, resize, pick. Expected non-zero, but far below the count above.
    uint64_t framesRenderedSynchronously = 0;

    /// Fence Signal() failures. The composite refused those frames.
    uint64_t fenceSignalFailures = 0;

    /// Composites that gave up waiting on the fence: the GPU was slow.
    uint64_t fenceWaitTimeouts = 0;

    /// DiscardInFlight() calls, which discard in-flight work so a frame rendered before a
    /// pick or resize cannot be composited after it.
    uint64_t drains = 0;

    /// WaitForQuiescence() calls, which keep the in-flight frame rather than
    /// discarding it.
    uint64_t quiescenceWaits = 0;

    /// Drains or quiescence waits that gave up before the thread went idle, so
    /// a discard may be followed by a late composite. Expected to be zero.
    uint64_t drainTimeouts = 0;

    /// Settles that completed, so displayed equals current after playback.
    uint64_t settles = 0;

    /// Redraws scheduled because the frame being settled to had not landed yet.
    uint64_t settleRetries = 0;

    /// Settles abandoned after kMaxSettleRetries; the viewport may be stale.
    uint64_t settlesAbandoned = 0;

    /// Render ops that applied a time change outside playback and composited
    /// their own frame instead of the previous one.
    uint64_t timeChangesComposited = 0;

    /// Composites with no depth AOV to blit. The colour blit still succeeds, so
    /// main-thread overlays sort against stale depth. Expected to be zero.
    uint64_t presentsWithoutDepth = 0;

    /// Composites refused for a zero-area region: a frozen viewport.
    uint64_t presentsSkipped = 0;

    /// Frames never published because the AOV copy failed. Expected to be zero.
    uint64_t aovCopyFailures = 0;

    /// Copies that found a composite-done fence on their pair and waited on it
    /// before overwriting it (the write-after-read half of the interlock). A
    /// pair is composited exactly once between two of its own writes, so this
    /// should track framesRenderedOffThread; a shortfall means frames are being
    /// rewritten unprotected. The wait is GPU-side and costs no CPU.
    uint64_t copyTargetWaits = 0;

    /// Composite-done fences that existed but could not be waited on, leaving
    /// that pair's next write unordered against the blit reading it. Tearing
    /// window. Expected to be zero.
    uint64_t copyTargetWaitFailures = 0;

    /// Composite-done fences that could not be created after the blit, so the
    /// pair went back to the render thread unprotected. Also a tearing window,
    /// but blames the main thread's context rather than the render thread's.
    uint64_t compositeFenceSignalFailures = 0;

    /// Composites that showed the previous frame again because the newest
    /// frame's fence was not ready. A steady stream means the thread is behind.
    uint64_t compositeReusedLastGood = 0;

    /// Interlock waits that ran past the warning point, such as the first
    /// render of a heavy scene. The push still waited, so these are slow
    /// frames rather than races.
    uint64_t idleWaitTimeouts = 0;

    /// GPU waits around copied GPU shared buffers that had no usable fence:
    /// a render that may read a buffer before Maya's copy into it lands, or a
    /// copy that may overwrite a buffer the last render is still reading.
    /// Either is torn geometry. Expected to be zero.
    uint64_t sharedBufferFenceFailures = 0;

    // Overlap measures slower than the flag-off path on a heavy scene (25 fps
    // against 40), so the interlock and the AOV copy are timed separately.

    /// Total main-thread time blocked in WaitForIdle: the parallelism given up.
    uint64_t idleWaitNanos = 0;

    /// Total render-thread CPU time issuing the AOV copy, not the GPU time
    /// performing it: glCopyImageSubData returns long before the copy
    /// completes, so the GPU cost lands on the frame's fence instead.
    uint64_t aovCopyNanos = 0;

    /// Total main-thread time in CompositeLatest, fence wait included.
    uint64_t compositeNanos = 0;

    /// Time waiting for a completed frame to exist: the render thread is behind.
    uint64_t compositeHandoffNanos = 0;

    /// Time waiting on the completed frame's GPU fence: the GPU is behind.
    uint64_t compositeFenceNanos = 0;

    /// Time issuing the colour+depth blit into Maya's framebuffer.
    uint64_t compositeBlitNanos = 0;

    /// The idle interlock was bypassed by MAYAHYDRA_OVERLAP_NO_INTERLOCK, so a
    /// benchmark cannot mislabel which configuration it measured.
    bool interlockBypassed = false;

    /// The AOV copy was bypassed by MAYAHYDRA_OVERLAP_NO_AOV_COPY.
    bool aovCopyBypassed = false;
};

/// Process-wide render-thread counters. A singleton like SnapshotManager,
/// because the reader is a Maya command with no route to the render override.
class MAYAHYDRALIB_API RenderThreadStats
{
public:
    static RenderThreadStats& instance();

    /// Read all counters. Not a consistent snapshot across fields, since a lock
    /// on the render thread's hot path is not worth it: compare two reads around
    /// a known amount of work rather than fields against each other.
    RenderThreadStatsSnapshot Get() const;

    /// Zero the counters, leaving the state flags alone.
    void Reset();

    void SetOverlapEnabled(bool value) { _overlapEnabled.store(value, std::memory_order_release); }
    void SetStarted(bool value) { _started.store(value, std::memory_order_release); }
    void SetContextArenaAcquired(bool value)
    {
        _contextArenaAcquired.store(value, std::memory_order_release);
    }
    void SetOwnsSurface(bool value) { _ownsSurface.store(value, std::memory_order_release); }

    void RecordFrameRenderedOffThread()
    {
        _framesRenderedOffThread.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordFrameComposited() { _framesComposited.fetch_add(1, std::memory_order_relaxed); }
    void RecordFrameRenderedSynchronously()
    {
        _framesRenderedSynchronously.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordFenceSignalFailure()
    {
        _fenceSignalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordFenceWaitTimeout() { _fenceWaitTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordDrain() { _drains.fetch_add(1, std::memory_order_relaxed); }
    void RecordQuiescenceWait() { _quiescenceWaits.fetch_add(1, std::memory_order_relaxed); }
    void RecordDrainTimeout() { _drainTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordSettle() { _settles.fetch_add(1, std::memory_order_relaxed); }
    void RecordSettleRetry() { _settleRetries.fetch_add(1, std::memory_order_relaxed); }
    void RecordSettleAbandoned() { _settlesAbandoned.fetch_add(1, std::memory_order_relaxed); }
    void RecordTimeChangeComposited()
    {
        _timeChangesComposited.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordPresentWithoutDepth() { _presentsWithoutDepth.fetch_add(1, std::memory_order_relaxed); }
    void RecordPresentSkipped() { _presentsSkipped.fetch_add(1, std::memory_order_relaxed); }
    void RecordAovCopyFailure() { _aovCopyFailures.fetch_add(1, std::memory_order_relaxed); }
    void RecordCopyTargetWaited() { _copyTargetWaits.fetch_add(1, std::memory_order_relaxed); }
    void RecordCopyTargetWaitFailure()
    {
        _copyTargetWaitFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordCompositeFenceSignalFailure()
    {
        _compositeFenceSignalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordCompositeReusedLastGood()
    {
        _compositeReusedLastGood.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordIdleWaitTimeout() { _idleWaitTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordSharedBufferFenceFailure()
    {
        _sharedBufferFenceFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void AddIdleWaitNanos(uint64_t nanos)
    {
        _idleWaitNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void AddAovCopyNanos(uint64_t nanos)
    {
        _aovCopyNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void AddCompositeNanos(uint64_t nanos)
    {
        _compositeNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void AddCompositeHandoffNanos(uint64_t nanos)
    {
        _compositeHandoffNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void AddCompositeFenceNanos(uint64_t nanos)
    {
        _compositeFenceNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void AddCompositeBlitNanos(uint64_t nanos)
    {
        _compositeBlitNanos.fetch_add(nanos, std::memory_order_relaxed);
    }
    void SetInterlockBypassed(bool value)
    {
        _interlockBypassed.store(value, std::memory_order_release);
    }
    void SetAovCopyBypassed(bool value)
    {
        _aovCopyBypassed.store(value, std::memory_order_release);
    }
private:
    RenderThreadStats() = default;

    RenderThreadStats(const RenderThreadStats&)            = delete;
    RenderThreadStats& operator=(const RenderThreadStats&) = delete;

    std::atomic<bool>     _overlapEnabled{false};
    std::atomic<bool>     _started{false};
    std::atomic<bool>     _contextArenaAcquired{false};
    std::atomic<bool>     _ownsSurface{false};
    std::atomic<uint64_t> _framesRenderedOffThread{0};
    std::atomic<uint64_t> _framesComposited{0};
    std::atomic<uint64_t> _framesRenderedSynchronously{0};
    std::atomic<uint64_t> _fenceSignalFailures{0};
    std::atomic<uint64_t> _fenceWaitTimeouts{0};
    std::atomic<uint64_t> _drains{0};
    std::atomic<uint64_t> _quiescenceWaits{0};
    std::atomic<uint64_t> _drainTimeouts{0};
    std::atomic<uint64_t> _settles{0};
    std::atomic<uint64_t> _settleRetries{0};
    std::atomic<uint64_t> _settlesAbandoned{0};
    std::atomic<uint64_t> _timeChangesComposited{0};
    std::atomic<uint64_t> _presentsWithoutDepth{0};
    std::atomic<uint64_t> _presentsSkipped{0};
    std::atomic<uint64_t> _aovCopyFailures{0};
    std::atomic<uint64_t> _copyTargetWaits{0};
    std::atomic<uint64_t> _copyTargetWaitFailures{0};
    std::atomic<uint64_t> _compositeFenceSignalFailures{0};
    std::atomic<uint64_t> _compositeReusedLastGood{0};
    std::atomic<uint64_t> _idleWaitTimeouts{0};
    std::atomic<uint64_t> _sharedBufferFenceFailures{0};
    std::atomic<uint64_t> _idleWaitNanos{0};
    std::atomic<uint64_t> _aovCopyNanos{0};
    std::atomic<uint64_t> _compositeNanos{0};
    std::atomic<uint64_t> _compositeHandoffNanos{0};
    std::atomic<uint64_t> _compositeFenceNanos{0};
    std::atomic<uint64_t> _compositeBlitNanos{0};
    std::atomic<bool>     _interlockBypassed{false};
    std::atomic<bool>     _aovCopyBypassed{false};
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_STATS_H
