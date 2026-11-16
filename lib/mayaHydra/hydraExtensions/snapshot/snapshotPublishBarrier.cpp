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

#include "snapshotPublishBarrier.h"

#include "snapshotEnvSettings.h"

PXR_NAMESPACE_OPEN_SCOPE

SnapshotManager& SnapshotManager::instance()
{
    static SnapshotManager sInstance;
    return sInstance;
}

SnapshotManager::SnapshotManager() = default;

void SnapshotManager::Initialize()
{
    // Guarded on _initialized, not _enabled, so the env setting is read only once.
    if (_initialized) {
        return;
    }
    _initialized = true;

    _enabled = ::MAYAHYDRA_NS_DEF::IsSnapshotEnabled();
}

SnapshotFrameId SnapshotManager::AdvanceFrame()
{
    if (!_enabled) {
        return 0;
    }
    return _currentFrameId.fetch_add(1, std::memory_order_acq_rel) + 1;
}

void SnapshotManager::PublishBarrier()
{
    if (!_enabled) {
        return;
    }

    const SnapshotFrameId current = _currentFrameId.load(std::memory_order_acquire);

    // Frame 0 is never published: it is the "no frame produced yet" value a consumer
    // sees before the first flush.
    if (current == 0 || _publishedFrameId.load(std::memory_order_relaxed) == current) {
        return;
    }

    // Release ordering so a consumer that observes this frame id also observes
    // everything the main thread produced for it.
    _publishedFrameId.store(current, std::memory_order_release);
    _publishCount.fetch_add(1, std::memory_order_relaxed);
}

SnapshotManager::Stats SnapshotManager::GetStats() const
{
    Stats s;
    s.enabled          = _enabled;
    s.currentFrameId   = _currentFrameId.load(std::memory_order_acquire);
    s.publishedFrameId = _publishedFrameId.load(std::memory_order_acquire);
    s.publishCount     = _publishCount.load(std::memory_order_relaxed);
    return s;
}

PXR_NAMESPACE_CLOSE_SCOPE
