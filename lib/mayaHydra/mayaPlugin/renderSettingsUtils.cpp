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

#include <mayaHydraLib/mayaUtils.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

#include <mayaUsdAPI/utils.h>

#include <maya/MAnimControl.h>
#include <maya/MCommonRenderSettingsData.h>
#include <maya/MRenderUtil.h>
#include <maya/MTime.h>

#include <ufe/runTimeMgr.h>
#include <ufe/sceneSegmentHandler.h>
#include <ufe/pathString.h>

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

// Should the HdArnold renderer use Hydra v1 render settings (render settings
// map, MayaHydra writes render product files) or Hydra v2 render settings
// (render settings in Hydra scene, renderer writes render product files).
bool HdArnoldUseHydraV2RenderSettings(const TfToken& rendererName)
{
    const std::string rendererNameStr = rendererName.GetString();
    const bool isHdArnoldRenderer = rendererNameStr.rfind("HdArnoldRendererPlugin", 0) == 0;
    if (!isHdArnoldRenderer) {
        return false;
    }

    // Default is use Hydra v1 render settings.
    return TfGetenvBool("MAYA_HYDRA_HD_ARNOLD_HYDRA_V2_RENDER_SETTINGS", false);
}

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
    const auto rsAppPath = GetActiveRenderSettingsAppPath();
    const auto stage     = MayaUsdAPI::getStage(rsAppPath);
    return FindUsdRenderSettingsOnStage(stage, usdRenderSettings) ?
        rsAppPath : Ufe::Path();
}

// Read the RenderSettingsType from the render delegate (renderer)
RenderSettingsType ReadRenderSettingsTypeFromRenderDelegate(const TfToken& rendererName)
{
    // Hardcoded at this time the logic to choose the RenderSettingsType.

    if (IsPrmanRenderSettingsDriveRenderPassEnabled(rendererName)
        || HdArnoldUseHydraV2RenderSettings(rendererName)
        ) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                     "Using Hydra v2 render settings.\n");
        return RenderSettingsType::HydraV2;
    }
    
    // Check if the scene contains Usd render settings
    UsdRenderSettings dummyUsdRenderSettings;// Pass a dummy UsdRenderSettings to just check for presence
    const auto psPath = ExtractUsdRenderSettingsFromScene(dummyUsdRenderSettings); 
    if (!psPath.empty()) {
        TF_DEBUG_MSG(MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                     "Using Hydra v1 render settings.\n");
        return RenderSettingsType::HydraV1;
    }

    TF_WARN("No USD render settings found, or USD render settings had no render products.");
    return RenderSettingsType::Unknown;
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

    // USD documentation
    // https://openusd.org/release/user_guides/schemas/usdRender/RenderSettings.html#properties
    // says that if no render products are supplied, renderer should still
    // output an image. At least one renderer (Hydra Arnold) does not do this
    // and renders nothing.  Catch the no render products case and return
    // false, so that Maya render settings default is used.
    auto hasProducts = [](const UsdRenderSettings& rs) {
        SdfPathVector targets;
        return rs.GetProductsRel().GetTargets(&targets) && !targets.empty();
    };

    // This is when at the global level of a usd file/stage is defined the render settings in renderSettingsPrimPath such as :
    //  renderSettingsPrimPath = "/Render/Settings"
    outSettings = UsdRenderSettings::GetStageRenderSettings(stage);
    if (outSettings.GetPrim().IsValid() && hasProducts(outSettings)) {
        return true;
    }

    UsdPrimRange range = stage->Traverse();
    for (UsdPrim prim : range) {
        if (prim.GetTypeName() == TfToken("RenderSettings")) {
            outSettings = UsdRenderSettings(prim);
            if (outSettings.GetPrim().IsValid() && hasProducts(outSettings)) {
                return true;
            }
        }
    }

    return false;
}

Ufe::Path GetActiveRenderSettingsAppPath()
{
    constexpr const char* attrName   = "activeSettingsPath";

    // Get the active render settings from the activeSettingsPath attribute
    // on the UsdDefaultRenderSettings node.
    MObject nodeObj;
    if (!TF_VERIFY(GetDependNodeFromNodeName(kUsdDefaultRenderSettingsNodeName.data(), nodeObj), "Could not find %s node.", kUsdDefaultRenderSettingsNodeName.data())) {
        return {};
    }

    MFnDependencyNode depNode(nodeObj);
    MPlug plug = depNode.findPlug(attrName, true);
    if (!TF_VERIFY(!plug.isNull(), "Could not find %s attribute on %s.", attrName, kUsdDefaultRenderSettingsNodeName.data())) {
        return {};
    }

    MString pathStr = plug.asString();
    if (!TF_VERIFY(pathStr.length() > 0, "%s attribute on %s is empty.", attrName, kUsdDefaultRenderSettingsNodeName.data())) {
        // Attribute is empty, provide a sensible fallback, the USD default
        // render settings themselves.
        constexpr const char* rsPrimPath = "/Render/SceneRenderSettings";
        pathStr = MString((std::string(kUsdDefaultRenderSettingsNodeName) + "," + rsPrimPath).c_str());
    }

    return Ufe::PathString::path(pathStr.asChar());
}

SdfPath GetActiveRenderSettingsHydraPath()
{
    // Use path mapper to map the active render settings application path
    // (Ufe::Path) to the Hydra SdfPath of its translation in the Hydra scene.
    auto ufePath = GetActiveRenderSettingsAppPath();
    auto hydraPath = Fvp::ufePathToPrimSelections(ufePath);

    // Render settings are not instanced, so there will be a single path.
    if (!TF_VERIFY(hydraPath.size() == 1, "Expected single path for active render settings.")) {
        return {};
    }
    return hydraPath[0].primPath;
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

RenderTimes::RenderTimes(
    bool         isAnimatedIn,
    const MTime& startTimeIn,
    const MTime& endTimeIn,
    float        timeIncrIn)
    : isAnimated(isAnimatedIn)
    , startTime(startTimeIn)
    , endTime(endTimeIn)
    , timeIncr(timeIncrIn)
{
}

// Single point of truth for render times is Maya default render globals, for
// all Hydra rendering types (Hydra v1 render settings, Hydra v2 render settings).
RenderTimes GetRenderTimes()
{
    MCommonRenderSettingsData mayaRenderSettings;
    MRenderUtil::getCommonRenderSettings(mayaRenderSettings);

    if (!mayaRenderSettings.isAnimated()) {
        const auto currentTime = MAnimControl::currentTime();
        return RenderTimes(false, currentTime, currentTime, 1.0f);
    }

    // Guard against a non-positive frame increment: consumers iterate
    // for (t = start; t <= end; t += timeIncr), so a zero or negative
    // frameBy would loop forever. Fall back to a 1-frame step.
    const float frameBy = mayaRenderSettings.frameBy > 0.0
        ? static_cast<float>(mayaRenderSettings.frameBy)
        : 1.0f;

    return RenderTimes(
        true,
        mayaRenderSettings.frameStart,
        mayaRenderSettings.frameEnd,
        frameBy);
}

} // namespace MAYAHYDRA_NS_DEF
