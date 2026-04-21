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
#include <maya/MStatus.h>
#include <maya/MTime.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdRender/settings.h>

#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

bool HydraRenderCmd::hydraRenderFromHydraV2RenderSettings()
{
    if (!_batchRenderer) {
        TF_WARN("hydraRenderFromHydraV2RenderSettings: _batchRenderer is a nullptr.\n");
        return false;
    }

    UsdRenderSettings usdRenderSettings;
    const auto psPath = ExtractUsdRenderSettingsFromScene(usdRenderSettings);
    if (psPath.empty()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No USD render settings found in Maya USD proxy shapes.\n");
        return false;
    }

    std::vector<MTime> renderTimes;
    const auto renderSettingsStage = usdRenderSettings.GetPrim().GetStage();
    if (renderSettingsStage) {
        renderTimes = GetRenderTimesFromStage(renderSettingsStage);
    }

    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Render times count: %zu\n",
        renderTimes.size());

    //We must iterate over all render times.
    for (const MTime& time : renderTimes) {
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
