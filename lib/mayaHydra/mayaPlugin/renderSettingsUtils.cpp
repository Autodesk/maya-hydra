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

#include "renderSettingsUtils.h"

#include <mayaUsdAPI/utils.h>

#include <ufe/runTimeMgr.h>
#include <ufe/sceneSegmentHandler.h>

#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>

#include <algorithm>
#include <cstdlib>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

    // returns true if the env var HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS is set to true
// and the renderer is PRman
bool IsPrmanRenderSettingsDriveRenderPassEnabled(const TfToken& rendererName)
{
    const std::string rendererNameStr = rendererName.GetString();
    const bool isPrmanRenderer = rendererNameStr.rfind("HdPrmanLoaderRendererPlugin", 0) == 0;
    if (!isPrmanRenderer) {
        return false;
    }

    return TfGetenvBool("HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS", false);
}

}
namespace MAYAHYDRA_NS_DEF {

// Extract render settings from all mayaUsdProxyShape nodes
// returns true if found
bool ExtractUsdRenderSettingsFromMayaUsdProxyShapes(UsdRenderSettings& usdRenderSettings)
{
    // Find all mayaUsdProxyShape nodes in the scene
    Ufe::SceneItemList proxyShapes = GetAllMayaUsdProxyShapes();

    // Process each proxy shape
    for (const auto& ps : proxyShapes) {
        const auto psPath = ps->path();
        const auto stage = MayaUsdAPI::getStage(psPath);
        if (FindUsdRenderSettingsOnStage(stage, usdRenderSettings)) {
            return true;
        }
    }

    return false;
}

// Read the RenderSettingsType from the render delegate (renderer)
RenderSettingsType ReadRenderSettingsTypeFromRenderDelegate(const TfToken& rendererName)
{
    // Hardcoded at this time the logic to choose the RenderSettingsType.

    if (IsPrmanRenderSettingsDriveRenderPassEnabled(rendererName)) {
        return RenderSettingsType::HydraV2;
    }
    
    // Check if the scene contains Usd render settings
    UsdRenderSettings dummyUsdRenderSettings;// Pass a dummy UsdRenderSettings to just check for presence
    const bool bHydraV1RenderSettingsAvailable = ExtractUsdRenderSettingsFromMayaUsdProxyShapes(
        dummyUsdRenderSettings); 
    if (bHydraV1RenderSettingsAvailable) {
        return RenderSettingsType::HydraV1;
    }

    return RenderSettingsType::Maya;
}
        
Ufe::SceneItemList GetAllMayaUsdProxyShapes()
{
    const auto mayaSceneSegmentHandler
        = Ufe::RunTimeMgr::instance().sceneSegmentHandler(MayaUsdAPI::getMayaRunTimeId());
    const auto mayaRootPath = mayaSceneSegmentHandler->rootSceneSegmentRootPath();
    const auto gatewayItems
        = Ufe::SceneSegmentHandler::findGatewayItems(mayaRootPath, MayaUsdAPI::getUsdRunTimeId());
    
    Ufe::SceneItemList proxyShapes;
    std::copy(
        gatewayItems.begin(),
        gatewayItems.end(),
        std::back_inserter(proxyShapes)
    );
    
    return proxyShapes;
}

bool FindUsdRenderSettingsOnStage(
    const UsdStageRefPtr& stage,
    UsdRenderSettings&    outSettings)
{
    if (!stage) {
        return false;
    }

    // This is when at the global level of a usd file/stage is defined the render settings in renderSettingsPrimPath such as :
    //  renderSettingsPrimPath = "/Render/Settings"
    outSettings = UsdRenderSettings::GetStageRenderSettings(stage);
    if (outSettings.GetPrim().IsValid()) {
        return true;
    }

    UsdPrimRange range = stage->Traverse();
    for (UsdPrim prim : range) {
        if (prim.GetTypeName() == TfToken("RenderSettings")) {
            outSettings = UsdRenderSettings(prim);
            if (outSettings.GetPrim().IsValid()) {
                return true;
            }
        }
    }

    return false;
}

} // namespace MAYAHYDRA_NS_DEF
