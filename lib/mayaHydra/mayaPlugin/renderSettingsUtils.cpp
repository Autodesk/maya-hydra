//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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

#include "pluginDebugCodes.h"

#include <mayaUsdAPI/utils.h>

#include <maya/MTime.h>

#include <ufe/runTimeMgr.h>
#include <ufe/sceneSegmentHandler.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/getenv.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/renderSettings.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/utils.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/primRange.h>
#include <pxr/usd/usdRender/tokens.h>

#include <algorithm>
#include <cstdlib>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

/* For future use
bool IsHydraArnoldRenderDelegate(const TfToken& rendererName)
{
    const std::string rendererNameStr = rendererName.GetString();
    const bool isHdArnoldRenderer = rendererNameStr.rfind("HdArnoldRendererPlugin", 0) == 0;
    return isHdArnoldRenderer;
}
*/

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

Ufe::Path ExtractUsdRenderSettingsFromScene(UsdRenderSettings& usdRenderSettings)
{
    // Find all mayaUsdProxyShape nodes in the scene
    Ufe::SceneItemList proxyShapes = GetAllMayaUsdProxyShapes();

    // Process each proxy shape
    for (const auto& ps : proxyShapes) {
        const auto psPath = ps->path();
        const auto stage = MayaUsdAPI::getStage(psPath);
        if (FindUsdRenderSettingsOnStage(stage, usdRenderSettings)) {
            return psPath;
        }
    }

    return Ufe::Path();
}

// Read the RenderSettingsType from the render delegate (renderer)
RenderSettingsType ReadRenderSettingsTypeFromRenderDelegate(const TfToken& rendererName)
{
    // Hardcoded at this time the logic to choose the RenderSettingsType.

    if (IsPrmanRenderSettingsDriveRenderPassEnabled(rendererName)
        /*
        || IsHydraArnoldRenderDelegate(rendererName)*/
        ) {
        return RenderSettingsType::HydraV2;
    }
    
    // Check if the scene contains Usd render settings
    UsdRenderSettings dummyUsdRenderSettings;// Pass a dummy UsdRenderSettings to just check for presence
    const auto psPath = ExtractUsdRenderSettingsFromScene(dummyUsdRenderSettings); 
    if (!psPath.empty()) {
        return RenderSettingsType::HydraV1;
    }

    return RenderSettingsType::Maya;
}
        
Ufe::SceneItemList GetAllMayaUsdProxyShapes()
{
    Ufe::SceneItemList proxyShapes;

    const auto mayaSceneSegmentHandler
        = Ufe::RunTimeMgr::instance().sceneSegmentHandler(MayaUsdAPI::getMayaRunTimeId());
    if (!mayaSceneSegmentHandler) {
        return proxyShapes;
    }
    const auto mayaRootPath = mayaSceneSegmentHandler->rootSceneSegmentRootPath();
    const auto gatewayItems
        = Ufe::SceneSegmentHandler::findGatewayItems(mayaRootPath, MayaUsdAPI::getUsdRunTimeId());
    
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

std::vector<MTime> GetRenderTimesFromStage(const UsdStageRefPtr& stage)
{
    std::vector<MTime> times;
    if (!stage || !stage->HasAuthoredTimeCodeRange()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "USD stage has no authored time range; returning empty.\n");
        return {};
    }

    const double startTimeCode = stage->GetStartTimeCode();
    const double endTimeCode = stage->GetEndTimeCode();
    const double timeCodesPerSecond = stage->GetTimeCodesPerSecond();
    const double framesPerSecond = stage->GetFramesPerSecond();

    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "USD time range from stage: start=%f end=%f tcps=%f fps=%f\n",
        startTimeCode,
        endTimeCode,
        timeCodesPerSecond,
        framesPerSecond);

    if (endTimeCode < startTimeCode) {
        TF_WARN("GetRenderTimesFromStage: USD time range invalid; "
                "endTimeCode < startTimeCode.\n");
        return times;
    }

    const double timeStep = 1.0;
    for (double timeCode = startTimeCode; timeCode <= endTimeCode; timeCode += timeStep) {
        times.emplace_back(timeCode, MTime::uiUnit());
    }

    return times;
}

SdfPath GetActiveRenderSettingsPrimHydraPathFromScene()
{
    UsdRenderSettings usdRenderSettings;
    auto psPath = ExtractUsdRenderSettingsFromScene(usdRenderSettings);
    if (psPath.empty()) {
        TF_WARN("No USD render settings found in scene");
        return {};
    }

    const auto usdRsPath = usdRenderSettings.GetPrim().GetPath();

    if (usdRsPath.IsEmpty()) {
        TF_WARN("Invalid path for USD render settings.");
        return {};
    }

    const auto hydraRsPath = usdRsPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), SdfPath("/MayaUsdProxyShape_PluginNode").AppendElementString(psPath.back().string()));

    return hydraRsPath;
}

TfTokenVector GetRenderOutputsFromActiveRenderSettings(const HdRenderIndex* renderIndex)
{
    TfTokenVector renderOutputs;
#if PXR_VERSION >= 2308
    if (!renderIndex) {
        return renderOutputs;
    }

    const HdSceneIndexBaseRefPtr terminalSceneIndex = renderIndex->GetTerminalSceneIndex();
    SdfPath renderSettingsPath;
    if (!HdUtils::HasActiveRenderSettingsPrim(terminalSceneIndex, &renderSettingsPath)) {
        return renderOutputs;
    }

    HdBprim* const bprim
        = renderIndex->GetBprim(HdPrimTypeTokens->renderSettings, renderSettingsPath);
    const auto* renderSettings = dynamic_cast<const HdRenderSettings*>(bprim);
    if (!renderSettings) {
        return renderOutputs;
    }

    for (const auto& product : renderSettings->GetRenderProducts()) {
        for (const auto& renderVar : product.renderVars) {
            TfToken varName = renderVar.varPath.GetNameToken();
            if (varName.IsEmpty() && !renderVar.sourceName.empty()) {
                varName = TfToken(renderVar.sourceName);
            }
            if (renderVar.sourceType == UsdRenderTokens->lpe) {
                const std::string lpeExpr = !renderVar.sourceName.empty()
                    ? renderVar.sourceName
                    : varName.GetString();
                if (!lpeExpr.empty()) {
                    varName = TfToken(std::string("lpe:") + lpeExpr);
                }
            } else if (!renderVar.sourceName.empty()) {
                if (renderVar.sourceName == "Ci") {
                    varName = HdAovTokens->color;
                } else if (renderVar.sourceName == "z") {
                    varName = HdAovTokens->depth;
                }
            }

            if (!varName.IsEmpty()
                && std::find(renderOutputs.begin(), renderOutputs.end(), varName)
                    == renderOutputs.end()) {
                renderOutputs.push_back(varName);
            }
        }
    }
#endif
    return renderOutputs;
}

} // namespace MAYAHYDRA_NS_DEF
