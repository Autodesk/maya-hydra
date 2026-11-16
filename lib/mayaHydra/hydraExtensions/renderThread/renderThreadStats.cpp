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
    out.overlapEnabled         = _overlapEnabled.load(std::memory_order_acquire);
    out.started                = _started.load(std::memory_order_acquire);
    out.contextArenaAcquired   = _contextArenaAcquired.load(std::memory_order_acquire);
    out.ownsSurface            = _ownsSurface.load(std::memory_order_acquire);
    out.framesRenderedOffThread = _framesRenderedOffThread.load(std::memory_order_relaxed);
    out.framesComposited       = _framesComposited.load(std::memory_order_relaxed);
    out.framesRenderedSynchronously
        = _framesRenderedSynchronously.load(std::memory_order_relaxed);
    out.fenceSignalFailures = _fenceSignalFailures.load(std::memory_order_relaxed);
    out.fenceWaitTimeouts   = _fenceWaitTimeouts.load(std::memory_order_relaxed);
    out.drains              = _drains.load(std::memory_order_relaxed);
    out.quiescenceWaits     = _quiescenceWaits.load(std::memory_order_relaxed);
    out.drainTimeouts       = _drainTimeouts.load(std::memory_order_relaxed);
    out.settles             = _settles.load(std::memory_order_relaxed);
    out.settleRetries       = _settleRetries.load(std::memory_order_relaxed);
    out.settlesAbandoned    = _settlesAbandoned.load(std::memory_order_relaxed);
    out.presentsWithoutDepth = _presentsWithoutDepth.load(std::memory_order_relaxed);
    out.presentsSkipped     = _presentsSkipped.load(std::memory_order_relaxed);
    out.aovCopyFailures     = _aovCopyFailures.load(std::memory_order_relaxed);
    out.copyTargetWaits     = _copyTargetWaits.load(std::memory_order_relaxed);
    out.copyTargetWaitFailures
        = _copyTargetWaitFailures.load(std::memory_order_relaxed);
    out.compositeFenceSignalFailures
        = _compositeFenceSignalFailures.load(std::memory_order_relaxed);
    out.compositeReusedLastGood = _compositeReusedLastGood.load(std::memory_order_relaxed);
    out.idleWaitTimeouts    = _idleWaitTimeouts.load(std::memory_order_relaxed);
    out.sharedBufferFenceFailures
        = _sharedBufferFenceFailures.load(std::memory_order_relaxed);
    out.idleWaitNanos       = _idleWaitNanos.load(std::memory_order_relaxed);
    out.aovCopyNanos        = _aovCopyNanos.load(std::memory_order_relaxed);
    out.compositeNanos      = _compositeNanos.load(std::memory_order_relaxed);
    out.compositeHandoffNanos = _compositeHandoffNanos.load(std::memory_order_relaxed);
    out.compositeFenceNanos   = _compositeFenceNanos.load(std::memory_order_relaxed);
    out.compositeBlitNanos    = _compositeBlitNanos.load(std::memory_order_relaxed);
    out.interlockBypassed   = _interlockBypassed.load(std::memory_order_acquire);
    out.aovCopyBypassed     = _aovCopyBypassed.load(std::memory_order_acquire);
    return out;
}

void
RenderThreadStats::Reset()
{
    _framesRenderedOffThread.store(0, std::memory_order_relaxed);
    _framesComposited.store(0, std::memory_order_relaxed);
    _framesRenderedSynchronously.store(0, std::memory_order_relaxed);
    _fenceSignalFailures.store(0, std::memory_order_relaxed);
    _fenceWaitTimeouts.store(0, std::memory_order_relaxed);
    _drains.store(0, std::memory_order_relaxed);
    _quiescenceWaits.store(0, std::memory_order_relaxed);
    _drainTimeouts.store(0, std::memory_order_relaxed);
    _settles.store(0, std::memory_order_relaxed);
    _settleRetries.store(0, std::memory_order_relaxed);
    _settlesAbandoned.store(0, std::memory_order_relaxed);
    _presentsWithoutDepth.store(0, std::memory_order_relaxed);
    _presentsSkipped.store(0, std::memory_order_relaxed);
    _aovCopyFailures.store(0, std::memory_order_relaxed);
    _copyTargetWaits.store(0, std::memory_order_relaxed);
    _copyTargetWaitFailures.store(0, std::memory_order_relaxed);
    _compositeFenceSignalFailures.store(0, std::memory_order_relaxed);
    _compositeReusedLastGood.store(0, std::memory_order_relaxed);
    _idleWaitTimeouts.store(0, std::memory_order_relaxed);
    _sharedBufferFenceFailures.store(0, std::memory_order_relaxed);
    _idleWaitNanos.store(0, std::memory_order_relaxed);
    _aovCopyNanos.store(0, std::memory_order_relaxed);
    _compositeNanos.store(0, std::memory_order_relaxed);
    _compositeHandoffNanos.store(0, std::memory_order_relaxed);
    _compositeFenceNanos.store(0, std::memory_order_relaxed);
    _compositeBlitNanos.store(0, std::memory_order_relaxed);
}

} // namespace MAYAHYDRA_NS_DEF
