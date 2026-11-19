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
// the render thread breaks. The counters below are the assertions instead.

#include <mayaHydraLib/api.h>

#include <atomic>
#include <cstdint>
#include <thread>

namespace MAYAHYDRA_NS_DEF {

/// A snapshot of the render-thread counters, read on the main thread.
struct RenderThreadStatsSnapshot
{
    /// The overlap flag is on; `started` says whether it took effect.
    bool overlapEnabled = false;

    /// Thread running with its context current, not the silent fallback.
    bool started = false;

    uint64_t framesRenderedOffThread = 0;

    /// Frames the main thread composited from the handoff.
    uint64_t framesComposited = 0;

    /// Frames the main thread rendered itself while the thread ran: first-frame
    /// prime, resize, pick. Expected non-zero, but far below the count above.
    uint64_t framesRenderedSynchronously = 0;

    /// Render ops that waited for the frame they submitted and composited it,
    /// rather than the previous one: a time change outside playback, or a
    /// different panel than the last frame was drawn for.
    uint64_t ownFramesComposited = 0;

    /// DiscardInFlight() calls, which discard in-flight work so a frame rendered
    /// before a pick, resize or change of panel cannot be composited after it.
    uint64_t drains = 0;

    /// Interlock waits that ran past the reporting point, such as the first
    /// render of a heavy scene. The push still waited, so these are slow
    /// frames rather than races.
    uint64_t idleWaitTimeouts = 0;

    // Failure modes. Each still leaves a believable image on screen, so a
    // test asserts these are zero rather than comparing pixels.

    /// Frames whose GPU fence could not be created.
    uint64_t fenceSignalFailures = 0;

    /// Composites whose frame had no usable GPU fence, and quiescence waits
    /// whose frame's GPU work did not complete within the budget.
    uint64_t fenceWaitTimeouts = 0;

    /// Quiescence waits that gave up before the thread went idle.
    uint64_t drainTimeouts = 0;

    /// Composites with no depth AOV, so main-thread overlays sort against
    /// stale depth.
    uint64_t presentsWithoutDepth = 0;

    /// Composites refused for a zero-area region: a frozen viewport.
    uint64_t presentsSkipped = 0;

    /// Frames never published because the AOV copy failed.
    uint64_t aovCopyFailures = 0;

    /// Composite-done fences that could not be created after the blit, so the
    /// copy pair went back to the render thread unordered against that blit.
    uint64_t compositeFenceSignalFailures = 0;

    /// GPU waits around copied GPU shared buffers that had no usable fence:
    /// a render that may read a buffer before Maya's copy into it lands, or a
    /// copy that may overwrite a buffer the last render is still reading.
    uint64_t sharedBufferFenceFailures = 0;

    /// Prims the capture scene index copied, whole or in part. Zero while the
    /// render thread runs means the render thread reads the live scene.
    uint64_t primsCaptured = 0;

    /// MayaHydra's guarded Maya reads (DgAccessLock) made on the render thread.
    /// Each is a read of live Maya data the capture should have served.
    uint64_t mayaReadsOnRenderThread = 0;

    /// Capture holds, and the main-thread time spent in them and in the
    /// releases that follow, in microseconds.
    uint64_t captureHolds = 0;
    uint64_t captureHoldMicros = 0;
    uint64_t captureReleaseMicros = 0;
};

/// Process-wide render-thread counters. A singleton because the reader is a
/// Maya command with no route to the render override.
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

    void RecordFrameRenderedOffThread()
    {
        _framesRenderedOffThread.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordFrameComposited() { _framesComposited.fetch_add(1, std::memory_order_relaxed); }
    void RecordFrameRenderedSynchronously()
    {
        _framesRenderedSynchronously.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordOwnFrameComposited()
    {
        _ownFramesComposited.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordDrain() { _drains.fetch_add(1, std::memory_order_relaxed); }
    void RecordIdleWaitTimeout() { _idleWaitTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordFenceSignalFailure()
    {
        _fenceSignalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordFenceWaitTimeout() { _fenceWaitTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordDrainTimeout() { _drainTimeouts.fetch_add(1, std::memory_order_relaxed); }
    void RecordPresentWithoutDepth() { _presentsWithoutDepth.fetch_add(1, std::memory_order_relaxed); }
    void RecordPresentSkipped() { _presentsSkipped.fetch_add(1, std::memory_order_relaxed); }
    void RecordAovCopyFailure() { _aovCopyFailures.fetch_add(1, std::memory_order_relaxed); }
    void RecordCompositeFenceSignalFailure()
    {
        _compositeFenceSignalFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordSharedBufferFenceFailure()
    {
        _sharedBufferFenceFailures.fetch_add(1, std::memory_order_relaxed);
    }
    void RecordPrimCaptured() { _primsCaptured.fetch_add(1, std::memory_order_relaxed); }
    void RecordCaptureHold(uint64_t micros)
    {
        _captureHolds.fetch_add(1, std::memory_order_relaxed);
        _captureHoldMicros.fetch_add(micros, std::memory_order_relaxed);
    }
    void RecordCaptureRelease(uint64_t micros)
    {
        _captureReleaseMicros.fetch_add(micros, std::memory_order_relaxed);
    }

    /// Render thread, around each request. Records the main thread's id, so that
    /// reads from the render thread's worker threads count too.
    void SetRendering(bool value, std::thread::id mainThread)
    {
        _mainThread = mainThread;
        _rendering.store(value, std::memory_order_release);
    }

    /// Counts a guarded Maya read made while a request renders, on any thread
    /// but the main one.
    void RecordMayaRead()
    {
        if (_rendering.load(std::memory_order_acquire)
            && std::this_thread::get_id() != _mainThread) {
            _mayaReadsOnRenderThread.fetch_add(1, std::memory_order_relaxed);
        }
    }

private:
    RenderThreadStats() = default;

    RenderThreadStats(const RenderThreadStats&)            = delete;
    RenderThreadStats& operator=(const RenderThreadStats&) = delete;

    std::atomic<bool>     _overlapEnabled{false};
    std::atomic<bool>     _started{false};
    std::atomic<uint64_t> _framesRenderedOffThread{0};
    std::atomic<uint64_t> _framesComposited{0};
    std::atomic<uint64_t> _framesRenderedSynchronously{0};
    std::atomic<uint64_t> _ownFramesComposited{0};
    std::atomic<uint64_t> _drains{0};
    std::atomic<uint64_t> _idleWaitTimeouts{0};
    std::atomic<uint64_t> _fenceSignalFailures{0};
    std::atomic<uint64_t> _fenceWaitTimeouts{0};
    std::atomic<uint64_t> _drainTimeouts{0};
    std::atomic<uint64_t> _presentsWithoutDepth{0};
    std::atomic<uint64_t> _presentsSkipped{0};
    std::atomic<uint64_t> _aovCopyFailures{0};
    std::atomic<uint64_t> _compositeFenceSignalFailures{0};
    std::atomic<uint64_t> _sharedBufferFenceFailures{0};
    std::atomic<uint64_t> _primsCaptured{0};
    std::atomic<uint64_t> _mayaReadsOnRenderThread{0};
    std::atomic<uint64_t> _captureHolds{0};
    std::atomic<uint64_t> _captureHoldMicros{0};
    std::atomic<uint64_t> _captureReleaseMicros{0};
    std::atomic<bool>     _rendering{false};
    std::thread::id       _mainThread;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_STATS_H
