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

#ifndef MAYAHYDRA_RENDER_THREAD_SETTLE_H
#define MAYAHYDRA_RENDER_THREAD_SETTLE_H

// Settle/catch-up redraw: when EM stops producing, the viewport would keep
// showing the one-frame-stale async frame until something else caused a redraw.

#include <mayaHydraLib/api.h>

#include <atomic>
#include <functional>

namespace MAYAHYDRA_NS_DEF {

/// \class RenderThreadSettle
///
/// The render override calls OnRenderOpEntry() at the start of each render op.
/// When playback has stopped since the last call, the last completed frame is
/// composited and one synchronous settle redraw fires, so it completes before
/// Maya returns to idle.
///
/// Only the playing-to-stopped transition triggers a settle. A trigger based
/// on "the newest submitted frame has not been composited" self-sustains:
/// every render op submits, so each settle arms the next one.
class MAYAHYDRALIB_API RenderThreadSettle
{
public:
    /// Composites the latest completed frame into Maya's FBO; true if it did.
    using CompositeCallback = std::function<bool()>;

    /// Requests a redraw from Maya.
    using RequestRedrawCallback = std::function<void()>;

    RenderThreadSettle();
    ~RenderThreadSettle();

    RenderThreadSettle(const RenderThreadSettle&)            = delete;
    RenderThreadSettle& operator=(const RenderThreadSettle&) = delete;

    void Initialize(CompositeCallback compositeCallback,
                    RequestRedrawCallback requestRedrawCallback);

    /// Called at the start of each render op, with MAnimControl::isPlaying()
    /// and whether a completed frame is available. True if a settle fired.
    /// \p compositesOwnFrame says this render op composites the frame it
    /// submits, which supersedes any settle: compositing the settled frame as
    /// well would layer two frames into one image.
    bool OnRenderOpEntry(bool isPlaying, bool hasCompletedFrame, bool compositesOwnFrame = false);

    /// Called when MAnimControl::isPlaying() goes from true to false.
    bool OnPlaybackStop();

    /// Reset the settle state (e.g. on teardown).
    void Reset();

    /// The most consecutive redraws to schedule while waiting for the frame
    /// being settled to. The retry is self-scheduling, so without a bound a
    /// render thread that never produces the frame spins the viewport forever.
    static constexpr int kMaxSettleRetries = 20;

private:
    CompositeCallback       _compositeCallback;
    RequestRedrawCallback    _requestRedrawCallback;
    std::atomic<bool>        _wasPlaying{false};
    std::atomic<bool>        _settlePending{false};
    std::atomic<uint64_t>    _lastSettledFrameId{0};

    std::atomic<int>         _settleRetries{0};
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_SETTLE_H
