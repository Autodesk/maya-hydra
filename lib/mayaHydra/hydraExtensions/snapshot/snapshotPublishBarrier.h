//
// Copyright 2026 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//
// Frame publication boundary for eval/draw pipelining. Names each frame the main
// thread produces and marks when it is complete, so a consumer can tell which frame's
// scene state it is rendering and whether that state has since moved on.
//
// AdvanceFrame() at the start of a flush, PublishBarrier() once it has finished.
// Publishing is a release store, so a consumer that observes the new frame id also
// observes everything the main thread produced for it. In-flight depth is 1 and latest
// wins: if the producer outruns the consumer, intermediate frames are skipped.
//
// Carries frame identity only. Scene data reaches the consumer through the scene index.
// See doc/framePublication.md.

#ifndef MAYAHYDRA_SNAPSHOT_PUBLISH_BARRIER_H
#define MAYAHYDRA_SNAPSHOT_PUBLISH_BARRIER_H

#include <mayaHydraLib/api.h>

#include <pxr/pxr.h>

#include <atomic>
#include <cstdint>

PXR_NAMESPACE_OPEN_SCOPE

// Monotonic frame identifier, advanced once per flush.
using SnapshotFrameId = uint64_t;

// A singleton so the flush hooks can reach it without threading a handle through the
// render override.
class MAYAHYDRALIB_API SnapshotManager
{
public:
    static SnapshotManager& instance();

    // Reads the env flag. Called once at startup.
    void Initialize();

    // Main thread, at the start of a flush.
    SnapshotFrameId AdvanceFrame();

    // Main thread, once the flush has finished. Republishing an already-published
    // frame is skipped, so every site that might be the last one in a frame can call it.
    void PublishBarrier();

    // Consumers compare this against their in-flight frame to detect staleness.
    SnapshotFrameId GetPublishedFrameId() const
    {
        return _publishedFrameId.load(std::memory_order_acquire);
    }

    SnapshotFrameId GetCurrentFrameId() const
    {
        return _currentFrameId.load(std::memory_order_acquire);
    }

    bool IsEnabled() const { return _enabled; }

    // Read from a test via `mayaHydraTesting -snapshotStats`: a barrier that never runs
    // is otherwise indistinguishable from one that runs every frame.
    struct Stats
    {
        SnapshotFrameId currentFrameId{0};   // advanced per flush
        SnapshotFrameId publishedFrameId{0}; // last published frame
        uint64_t        publishCount{0};     // barriers that published
        bool            enabled{false};
    };
    Stats GetStats() const;

private:
    SnapshotManager();

    bool                         _enabled{false};
    bool                         _initialized{false};
    std::atomic<SnapshotFrameId> _currentFrameId{0};
    std::atomic<SnapshotFrameId> _publishedFrameId{0};
    std::atomic<uint64_t>        _publishCount{0};
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRA_SNAPSHOT_PUBLISH_BARRIER_H
