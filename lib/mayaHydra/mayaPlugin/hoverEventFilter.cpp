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

#include "hoverEventFilter.h"

#include <QEvent>
#include <QMouseEvent>
#include <QWidget>

#include <cmath>

namespace {

// Rounds a logical-pixel coordinate to a device-pixel coordinate.
int toDevicePixels(double logical, double devicePixelRatio)
{
    return static_cast<int>(std::lround(logical * devicePixelRatio));
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

HoverEventFilter::HoverEventFilter(QWidget* widget, PositionCallback callback)
    : _widget(widget)
    , _callback(std::move(callback))
{
    if (_widget) {
        // Without mouse tracking, move events arrive only while a button is held. Restored on
        // destruction.
        _mouseTrackingWasEnabled = _widget->hasMouseTracking();
        _widget->setMouseTracking(true);
        _widget->installEventFilter(this);
    }
}

HoverEventFilter::~HoverEventFilter()
{
    if (_widget) {
        _widget->removeEventFilter(this);
        _widget->setMouseTracking(_mouseTrackingWasEnabled);
    }
}

bool HoverEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _widget && _callback) {
        switch (event->type()) {
        // Press and release are handled like a move, so the hover turns off as soon as a click
        // starts (tumble, marquee) and back on at release, without waiting for the cursor to move.
        // buttons() already reflects the pressed or released button, so `active` is correct.
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            // Any held button means the user is dragging/tumbling, not hovering.
            const bool active = (mouseEvent->buttons() == Qt::NoButton);
            const double dpr = _widget->devicePixelRatioF();
            const QPointF pos = mouseEvent->position();
            _callback(toDevicePixels(pos.x(), dpr), toDevicePixels(pos.y(), dpr), active);
            break;
        }
        case QEvent::Leave:
            // Cursor left the viewport: clear hover.
            _callback(-1, -1, false);
            break;
        default:
            break;
        }
    }

    // Never consume the event: Maya's own interaction must proceed unaffected.
    return false;
}

} // namespace MAYAHYDRA_NS_DEF
