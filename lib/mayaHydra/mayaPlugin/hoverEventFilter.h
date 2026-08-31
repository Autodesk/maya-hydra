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
#include <QWidget>

#include <functional>

namespace MAYAHYDRA_NS_DEF {

/// \class HoverEventFilter
/// A passive Qt event filter installed on a Maya 3D viewport widget to track the mouse cursor
/// position for viewport hover highlighting. It never consumes events, so Maya's own interaction is
/// unaffected, and it enables mouse tracking so move events arrive with no button held.
///
/// Deliberately no Q_OBJECT macro: no signals or slots are needed, so this target does not have to
/// enable CMake's AUTOMOC. Adding one would require enabling it.
class HoverEventFilter : public QObject
{
public:
    /// Callback signature: (deviceX, deviceY, active). Coordinates are device pixels -- the logical
    /// position scaled by the widget's device-pixel ratio -- and keep Qt's top-left origin, which
    /// the consumer converts to Maya's bottom-left. \p active is true only for a genuine hover:
    /// cursor inside the widget with no button held. When false the coordinates are (-1, -1).
    using PositionCallback = std::function<void(int, int, bool)>;

    /// Installs the filter on \p widget and enables mouse tracking. The callback is
    /// invoked on the UI thread for each mouse-move and leave event.
    HoverEventFilter(QWidget* widget, PositionCallback callback);
    ~HoverEventFilter() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// QPointer, not a raw pointer: on plugin unload and Maya shutdown the widget can be destroyed
    /// before this filter is, and the destructor touches the widget.
    QPointer<QWidget> _widget;
    PositionCallback  _callback;
    bool              _mouseTrackingWasEnabled { false };
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_HOVER_EVENT_FILTER_H
