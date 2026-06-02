//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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
#include "hydraRenderCmd.h"

#include "batchRenderer.h"
#include "pluginDebugCodes.h"
#include "renderSettingsUtils.h"

#include <maya/MAnimControl.h>
#include <maya/MTime.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

bool HydraRenderCmd::hydraRenderFromHydraV2RenderSettings()
{
    if (!_batchRenderer) {
        TF_WARN("hydraRenderFromHydraV2RenderSettings: _batchRenderer is a nullptr.\n");
        return false;
    }

    const auto renderTimes = GetRenderTimes();
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Render time range: start=%.3f end=%.3f by=%.3f animated=%d\n",
        renderTimes.startTime.as(MTime::uiUnit()),
        renderTimes.endTime.as(MTime::uiUnit()),
        static_cast<double>(renderTimes.timeIncr),
        renderTimes.isAnimated);

    for (MTime time = renderTimes.startTime; time <= renderTimes.endTime; time += renderTimes.timeIncr) {
        if (MAnimControl::currentTime() != time) {
            MAnimControl::setCurrentTime(time);
        }

        if (_batchRenderer->RenderFromHydraV2RenderSettings() != MS::kSuccess) {
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                "BatchRenderer::RenderFromHydraV2RenderSettings failed.\n");
            return false;
        }
    }

    return true;
}

} // namespace MAYAHYDRA_NS_DEF
