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

#ifndef MAYAHYDRA_RENDER_THREAD_HANDOFF_H
#define MAYAHYDRA_RENDER_THREAD_HANDOFF_H

// Handoff protocol.
//
// Main signals "snapshot(frameId) ready"; the render thread signals
// "frame(frameId) complete" and carries the color and depth AOV handles plus a
// GPU fence (glFenceSync or Hgi) that main waits on before blitting. frameId
// comes from SnapshotManager. Depth-1 latest-wins: when eval outruns draw,
// intermediate snapshots are dropped by the snapshot publish barrier.

#include "renderThreadFence.h"

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
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgi/types.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>

namespace MAYAHYDRA_NS_DEF {

/// A completed frame's AOV handles + frameId + GPU fence.
/// Published by the render thread, consumed by the main thread through
/// WaitForCompletedFrame().
struct CompletedFrame
{
    /// The SnapshotManager frameId consumed by the render thread.
    uint64_t frameId{0};

    /// The color AOV texture handle. Valid until the next frame completes.
    pxr::HgiTextureHandle srcColor;

    /// The depth AOV texture handle (may be empty if no depth AOV).
    pxr::HgiTextureHandle srcDepth;

    /// The Hgi that produced the textures. Used by the present bridge to
    /// select the composite path (GL->GL or VK->GL).
    pxr::Hgi* srcHgi{nullptr};

    /// The render region (left, bottom, width, height) for the composite.
    pxr::GfVec4i dstRegion{0, 0, 0, 0};

    /// THIS frame's GPU fence, carried with the frame rather than read from a
    /// shared slot, so the main thread waits for the work that produced the
    /// handles above and not for whatever the render thread has since started.
    /// shared_ptr: the render thread may publish a newer frame and drop its
    /// reference while the main thread is still inside the wait.
    RenderThreadFencePtr fence;

    /// Which of the render thread's copy pairs these textures live in, or -1
    /// when they are not from a pair (the AOV-copy bypass). The main thread
    /// hands the index back so the write-after-read fence has a slot to go to.
    int copyIndex{-1};

    bool valid{false};
};

/// \class RenderThreadHandoff
///
/// Latest-wins depth-1 handoff from the render thread (producer) to the main
/// thread (consumer). A mutex guards the completed-frame slot and a condition
/// variable wakes the consumer. If a new frame is published before the main
/// thread consumes the previous one, the previous frame is dropped and its
/// AOVs are reclaimed by the next render.
class MAYAHYDRALIB_API RenderThreadHandoff
{
public:
    RenderThreadHandoff() = default;
    ~RenderThreadHandoff() = default;

    RenderThreadHandoff(const RenderThreadHandoff&)            = delete;
    RenderThreadHandoff& operator=(const RenderThreadHandoff&) = delete;

    /// Publish a completed frame. Called on the render thread once
    /// FramePass::Render completes and the frame's GPU fence is signalled.
    /// Unconditionally latest-wins: no consumer waits for a specific frame.
    void PublishCompletedFrame(const CompletedFrame& frame)
    {
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _completed       = frame;
            _completed.valid = true;
            _hasFrame.store(true, std::memory_order_release);
        }
        _cv.notify_one();
    }

    /// Wait up to \p timeout on the main thread for a completed frame, or
    /// return an invalid CompletedFrame. Blocks on a condition variable.
    CompletedFrame WaitForCompletedFrame(std::chrono::milliseconds timeout)
    {
        std::unique_lock<std::mutex> lock(_mutex);
        if (!_hasFrame.load(std::memory_order_acquire))
        {
            _cv.wait_for(lock, timeout, [this] {
                return _hasFrame.load(std::memory_order_acquire);
            });
        }
        if (_hasFrame.load(std::memory_order_acquire))
        {
            // The frame stays in the slot: the contract is "latest completed
            // frame", so it is composited again if no newer frame arrives.
            return _completed;
        }
        return CompletedFrame{};
    }

    /// The latest completed frame without waiting, or an invalid CompletedFrame.
    CompletedFrame TryGetCompletedFrame() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_hasFrame.load(std::memory_order_acquire))
        {
            return _completed;
        }
        return CompletedFrame{};
    }

    /// Returns the frameId of the latest completed frame, or 0 if none.
    uint64_t GetCompletedFrameId() const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        if (_hasFrame.load(std::memory_order_acquire))
        {
            return _completed.frameId;
        }
        return 0;
    }

    bool HasCompletedFrame() const
    {
        return _hasFrame.load(std::memory_order_acquire);
    }

    /// Drop the completed frame. Called by DiscardInFlight() so a frame
    /// rendered before a pick or resize cannot be composited after it.
    void Clear()
    {
        std::lock_guard<std::mutex> lock(_mutex);
        _completed = CompletedFrame{};
        _hasFrame.store(false, std::memory_order_release);
    }

private:
    mutable std::mutex      _mutex;
    std::condition_variable _cv;
    CompletedFrame          _completed;
    std::atomic<bool>       _hasFrame{false};
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_HANDOFF_H
