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

#include "mhHoverEventFilter.h"

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

MhHoverEventFilter::MhHoverEventFilter(QWidget* widget, PositionCallback callback)
    : _widget(widget)
    , _callback(std::move(callback))
{
    if (_widget) {
        // Without mouse tracking a widget only receives move events while a button is
        // held; hover needs button-less moves, so enable it (restored on destruction).
        _mouseTrackingWasEnabled = _widget->hasMouseTracking();
        _widget->setMouseTracking(true);
        _widget->installEventFilter(this);
    }
}

MhHoverEventFilter::~MhHoverEventFilter()
{
    if (_widget) {
        _widget->removeEventFilter(this);
        _widget->setMouseTracking(_mouseTrackingWasEnabled);
    }
}

bool MhHoverEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == _widget && _callback) {
        switch (event->type()) {
        case QEvent::MouseMove: {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            // Any held button means the user is dragging/tumbling, not hovering.
            const bool active = (mouseEvent->buttons() == Qt::NoButton);
            const double dpr   = _widget->devicePixelRatioF();
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            const QPointF pos = mouseEvent->position();
#else
            const QPointF pos = mouseEvent->localPos();
#endif
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
