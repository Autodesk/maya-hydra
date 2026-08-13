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
#ifndef MAYAHYDRA_HOVER_EVENT_FILTER_H
#define MAYAHYDRA_HOVER_EVENT_FILTER_H

#include <mayaHydraLib/mayaHydra.h>

#include <QObject>
#include <QPointer>
// QWidget is included rather than forward-declared: QPointer<QWidget> needs the complete type.
#include <QWidget>

#include <functional>

namespace MAYAHYDRA_NS_DEF {

/// \class MhHoverEventFilter
/// A passive Qt event filter installed on a Maya 3D viewport widget to track the
/// mouse cursor position for viewport hover highlighting.
///
/// It does not consume events (it always returns false) so Maya's own interaction
/// is unaffected. Mouse tracking is enabled on the widget so move events are
/// delivered even when no mouse button is held.
///
/// Cursor positions are reported through the supplied callback in device pixels
/// (logical position scaled by the widget's device-pixel ratio), keeping Qt's
/// top-left origin. The consumer converts to Maya's bottom-left origin; measured
/// on a real viewport, the widget height and the Hydra viewport height agree, so
/// there is nothing to gain from flipping here instead.
///
/// This deliberately avoids the Q_OBJECT macro (no signals/slots) so no moc step
/// is required; \ref eventFilter is a plain virtual override of QObject.
class MhHoverEventFilter : public QObject
{
public:
    /// Callback signature: (deviceX, deviceY, active). The coordinates are device
    /// pixels with Qt's top-left origin, as described above. \p active is true while
    /// the cursor is inside the viewport with no mouse button held (i.e. a genuine
    /// hover, not a drag/tumble); false otherwise, meaning "no hover", in which case
    /// the coordinates are (-1, -1).
    using PositionCallback = std::function<void(int, int, bool)>;

    /// Installs the filter on \p widget and enables mouse tracking. The callback is
    /// invoked on the UI thread for each mouse-move and leave event.
    MhHoverEventFilter(QWidget* widget, PositionCallback callback);
    ~MhHoverEventFilter() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// QPointer, not a raw pointer: nothing guarantees the viewport widget outlives this filter.
    /// The panel-deleted callback normally removes the filter first, but on plugin unload and Maya
    /// shutdown the widget can be destroyed before ~MtohRenderOverride destroys the filter map, and
    /// the destructor below touches the widget. QPointer auto-nulls when the widget goes away (Qt
    /// having already dropped the event filter), turning a use-after-free into a no-op.
    QPointer<QWidget> _widget;
    PositionCallback  _callback;
    bool              _mouseTrackingWasEnabled { false };
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_HOVER_EVENT_FILTER_H
