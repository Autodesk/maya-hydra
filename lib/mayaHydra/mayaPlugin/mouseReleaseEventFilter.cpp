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

#include "mouseReleaseEventFilter.h"

#include <QCoreApplication>
#include <QEvent>
#include <QMouseEvent>

namespace MAYAHYDRA_NS_DEF {

MouseReleaseEventFilter::MouseReleaseEventFilter(std::function<void()> callback)
    : _callback(std::move(callback))
{
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->installEventFilter(this);
    }
}

MouseReleaseEventFilter::~MouseReleaseEventFilter()
{
    if (QCoreApplication* app = QCoreApplication::instance()) {
        app->removeEventFilter(this);
    }
}

bool MouseReleaseEventFilter::eventFilter(QObject* watched, QEvent* event)
{
    // A release is delivered to every widget it propagates through; buttons() already excludes
    // the released button, so only the release of the last held one is reported, once per widget.
    if (event->type() == QEvent::MouseButtonRelease && _callback
        && static_cast<QMouseEvent*>(event)->buttons() == Qt::NoButton) {
        _callback();
    }
    // Never consume the event: Maya's own interaction must proceed unaffected.
    return false;
}

} // namespace MAYAHYDRA_NS_DEF
