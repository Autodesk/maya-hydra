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
/// Passive Qt event filter on a Maya viewport widget that reports the cursor position for hover
/// highlighting. It never consumes events.
///
/// No Q_OBJECT macro, so the target does not need CMake's AUTOMOC; adding one would require it.
class HoverEventFilter : public QObject
{
public:
    /// Callback signature: (deviceX, deviceY, active). Coordinates are device pixels with Qt's
    /// top-left origin; the consumer converts to Maya's bottom-left. \p active is true only when
    /// the cursor is inside the widget with no button held. On leave the coordinates are (-1, -1).
    using PositionCallback = std::function<void(int, int, bool)>;

    /// Installs the filter on \p widget and enables mouse tracking. The callback is invoked on the
    /// UI thread for mouse move, press, release and leave events.
    HoverEventFilter(QWidget* widget, PositionCallback callback);
    ~HoverEventFilter() override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    /// QPointer: the widget can be destroyed before this filter (plugin unload, Maya shutdown), and
    /// the destructor touches it.
    QPointer<QWidget> _widget;
    PositionCallback  _callback;
    bool              _mouseTrackingWasEnabled { false };
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_HOVER_EVENT_FILTER_H
