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

#include "renderThreadSettle.h"

#include "renderThreadStats.h"

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF
{

RenderThreadSettle::RenderThreadSettle() = default;

RenderThreadSettle::~RenderThreadSettle() = default;

void
RenderThreadSettle::Initialize(
    CompositeCallback       compositeCallback,
    RequestRedrawCallback   requestRedrawCallback)
{
    _compositeCallback = std::move(compositeCallback);
    _requestRedrawCallback = std::move(requestRedrawCallback);
}

bool
RenderThreadSettle::OnRenderOpEntry(bool isPlaying, bool hasCompletedFrame, bool compositesOwnFrame)
{
    bool firedSettle = false;

    const bool wasPlaying = _wasPlaying.load(std::memory_order_acquire);
    _wasPlaying.store(isPlaying, std::memory_order_release);

    if (compositesOwnFrame)
    {
        // The render op puts a newer frame on screen than the settle would, and
        // the composite blends over what is already in the framebuffer, so the
        // settled frame would show through it.
        _settlePending.store(false, std::memory_order_release);
        _settleRetries.store(0, std::memory_order_release);
    }
    else if (wasPlaying && !isPlaying)
    {
        firedSettle = OnPlaybackStop();
    }
    else if (!isPlaying && _settlePending.load(std::memory_order_acquire))
    {
        if (hasCompletedFrame && _compositeCallback && _compositeCallback())
        {
            _settlePending.store(false, std::memory_order_release);
            _settleRetries.store(0, std::memory_order_release);
            RenderThreadStats::instance().RecordSettle();
            firedSettle = true;
        }
        else
        {
            // The frame being settled to has not landed yet. Abandoning the
            // settle here would leave the viewport on frame N-1 with nothing
            // scheduled to correct it, so ask for another redraw instead.
            const int attempt = _settleRetries.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (attempt <= kMaxSettleRetries)
            {
                RenderThreadStats::instance().RecordSettleRetry();
                if (_requestRedrawCallback)
                {
                    _requestRedrawCallback();
                }
            }
            else
            {
                // Give up rather than spin: a possibly stale frame costs less
                // than an endlessly redrawing viewport.
                _settlePending.store(false, std::memory_order_release);
                _settleRetries.store(0, std::memory_order_release);
                RenderThreadStats::instance().RecordSettleAbandoned();
                TF_WARN("RenderThreadSettle: gave up settling after %d redraws "
                        "without the render thread completing a frame. The "
                        "viewport may be showing a stale frame.",
                        kMaxSettleRetries);
            }
        }
    }

    return firedSettle;
}

bool
RenderThreadSettle::OnPlaybackStop()
{
    // The composite happens on the next render op entry, which Maya runs for
    // the redraw requested below. The pending flag is what tells that entry to
    // composite even though no new request was submitted.
    _settlePending.store(true, std::memory_order_release);
    _settleRetries.store(0, std::memory_order_release);

    if (_requestRedrawCallback)
    {
        _requestRedrawCallback();
    }

    return true;
}

void
RenderThreadSettle::Reset()
{
    _wasPlaying.store(false, std::memory_order_release);
    _settlePending.store(false, std::memory_order_release);
    _lastSettledFrameId.store(0, std::memory_order_release);
    _settleRetries.store(0, std::memory_order_release);
}

} // namespace MAYAHYDRA_NS_DEF
