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

#include "renderThreadStats.h"

namespace MAYAHYDRA_NS_DEF {

RenderThreadStats&
RenderThreadStats::instance()
{
    static RenderThreadStats sInstance;
    return sInstance;
}

RenderThreadStatsSnapshot
RenderThreadStats::Get() const
{
    RenderThreadStatsSnapshot out;
    out.overlapEnabled          = _overlapEnabled.load(std::memory_order_acquire);
    out.started                 = _started.load(std::memory_order_acquire);
    out.framesRenderedOffThread = _framesRenderedOffThread.load(std::memory_order_relaxed);
    out.framesComposited        = _framesComposited.load(std::memory_order_relaxed);
    out.framesRenderedSynchronously
        = _framesRenderedSynchronously.load(std::memory_order_relaxed);
    out.ownFramesComposited  = _ownFramesComposited.load(std::memory_order_relaxed);
    out.drains               = _drains.load(std::memory_order_relaxed);
    out.idleWaitTimeouts     = _idleWaitTimeouts.load(std::memory_order_relaxed);
    out.fenceSignalFailures  = _fenceSignalFailures.load(std::memory_order_relaxed);
    out.fenceWaitTimeouts    = _fenceWaitTimeouts.load(std::memory_order_relaxed);
    out.drainTimeouts        = _drainTimeouts.load(std::memory_order_relaxed);
    out.presentsWithoutDepth = _presentsWithoutDepth.load(std::memory_order_relaxed);
    out.presentsSkipped      = _presentsSkipped.load(std::memory_order_relaxed);
    out.aovCopyFailures      = _aovCopyFailures.load(std::memory_order_relaxed);
    out.compositeFenceSignalFailures
        = _compositeFenceSignalFailures.load(std::memory_order_relaxed);
    out.sharedBufferFenceFailures
        = _sharedBufferFenceFailures.load(std::memory_order_relaxed);
    out.primsCaptured           = _primsCaptured.load(std::memory_order_relaxed);
    out.mayaReadsOnRenderThread = _mayaReadsOnRenderThread.load(std::memory_order_relaxed);
    out.captureHolds            = _captureHolds.load(std::memory_order_relaxed);
    out.captureHoldMicros       = _captureHoldMicros.load(std::memory_order_relaxed);
    out.captureReleaseMicros    = _captureReleaseMicros.load(std::memory_order_relaxed);
    return out;
}

void
RenderThreadStats::Reset()
{
    _framesRenderedOffThread.store(0, std::memory_order_relaxed);
    _framesComposited.store(0, std::memory_order_relaxed);
    _framesRenderedSynchronously.store(0, std::memory_order_relaxed);
    _ownFramesComposited.store(0, std::memory_order_relaxed);
    _drains.store(0, std::memory_order_relaxed);
    _idleWaitTimeouts.store(0, std::memory_order_relaxed);
    _fenceSignalFailures.store(0, std::memory_order_relaxed);
    _fenceWaitTimeouts.store(0, std::memory_order_relaxed);
    _drainTimeouts.store(0, std::memory_order_relaxed);
    _presentsWithoutDepth.store(0, std::memory_order_relaxed);
    _presentsSkipped.store(0, std::memory_order_relaxed);
    _aovCopyFailures.store(0, std::memory_order_relaxed);
    _compositeFenceSignalFailures.store(0, std::memory_order_relaxed);
    _sharedBufferFenceFailures.store(0, std::memory_order_relaxed);
    _primsCaptured.store(0, std::memory_order_relaxed);
    _mayaReadsOnRenderThread.store(0, std::memory_order_relaxed);
    _captureHolds.store(0, std::memory_order_relaxed);
    _captureHoldMicros.store(0, std::memory_order_relaxed);
    _captureReleaseMicros.store(0, std::memory_order_relaxed);
}

} // namespace MAYAHYDRA_NS_DEF
