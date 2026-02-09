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
#include "renderVarUtils.h"

#include <flowViewport/imageWriter/fvpImageBufferWriter.h>

#include <maya/MAnimControl.h>
#include <maya/MFileIO.h>
#include <maya/MFnRenderLayer.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MTime.h>

#include <mayaUsdAPI/utils.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <pxr/base/gf/vec2i.h>
#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/scoped.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/renderDelegate.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/layer.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usdRender/product.h>
#include <pxr/usd/usdRender/settings.h>
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usd/usdRender/var.h>

#include <algorithm>
#include <filesystem>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

using namespace MAYAHYDRA_NS_DEF;

// Apply time-sampled USD render settings to the render delegate.
bool ApplyUsdRenderSettingsToRenderDelegate(
    const UsdRenderSettings& usdRenderSettings,
    const UsdTimeCode&       timeCode,
    HdRenderDelegate*        renderDelegate)
{
    if (!usdRenderSettings) {
        TF_WARN("ApplyUsdRenderSettingsToRenderDelegate: UsdRenderSettings invalid; "
                "nothing to apply.\n");
        return false;
    }
    if (!renderDelegate) {
        TF_WARN("ApplyUsdRenderSettingsToRenderDelegate: Render delegate unavailable; "
                "cannot apply USD render settings.\n");
        return false;
    }

    size_t            appliedSettings = 0;
    for (const UsdAttribute& attr : usdRenderSettings.GetPrim().GetAttributes()) {
        VtValue value;
        if (!attr.Get(&value, timeCode) || value.IsEmpty()) {
            continue;
        }
        renderDelegate->SetRenderSetting(TfToken(attr.GetName()), value);
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Applied render setting: %s\n",
            attr.GetName().GetText());
        ++appliedSettings;
    }

    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Applied %zu USD render settings to render delegate.\n",
        appliedSettings);
    return appliedSettings > 0;
}

// When imageName is relative, resolve it against the USD stage root layer path.
std::string ResolveRenderProductImagePath(
    const std::string& imageName,
    const UsdStageRefPtr& stage)
{
    if (imageName.empty()) {
        return imageName;
    }

    std::filesystem::path imagePath(imageName);
    if (imagePath.is_absolute()) {
        return imageName;
    }

    std::filesystem::path baseDir;
    if (stage) {
        const SdfLayerHandle rootLayer = stage->GetRootLayer();
        if (rootLayer) {
            std::string rootLayerPath = rootLayer->GetRealPath();
            if (rootLayerPath.empty()) {
                rootLayerPath = rootLayer->GetIdentifier();
            }
            if (!rootLayerPath.empty()
                && rootLayerPath.rfind("anon:", 0) != 0) {
                baseDir = std::filesystem::path(rootLayerPath).parent_path();
            }
        }
    }

    if (baseDir.empty()) {
        const MString scenePath = MFileIO::currentFile();
        if (scenePath.length() > 0) {
            baseDir = std::filesystem::path(scenePath.asChar()).parent_path();
        }
    }

    if (baseDir.empty()) {
        return imageName;
    }

    return (baseDir / imagePath).lexically_normal().string();
}

Ufe::Path GetUfeCameraPathFromUsdRenderSettings(const UsdRenderSettings& usdRenderSettings)
{
    if (!usdRenderSettings) {
        TF_WARN("GetUfeCameraPathFromUsdRenderSettings: UsdRenderSettings invalid; "
                "no default camera.\n");

        return Ufe::Path();
    }

    UsdRelationship cameraRel = usdRenderSettings.GetCameraRel();
    SdfPathVector   cameraTargets;
    if (cameraRel.GetTargets(&cameraTargets) && !cameraTargets.empty()) {

        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Render settings camera target: %s\n",
            cameraTargets[0].GetText());
        const SdfPath usdCameraPath = cameraTargets[0];
        UsdStageRefPtr stage = usdRenderSettings.GetPrim().GetStage();
        auto stagePath = MayaUsdAPI::stagePath(stage);
        Ufe::Path ufeCameraPath
            = Ufe::Path::Segments { stagePath.getSegments()[0],
                                    MayaUsdAPI::usdPathToUfePathSegment(usdCameraPath) };
        return ufeCameraPath;
    }

    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "No camera relationship authored on render settings: %s\n",
        usdRenderSettings.GetPrim().GetPath().GetText());

    return Ufe::Path();
}

Ufe::Path GetUfeCameraPathFromUsdRenderProductOverride(
    const UsdRenderProduct&  renderProduct)
{
    Ufe::Path ufeCameraPath;
    if (!renderProduct) {
        TF_WARN("GetUfeCameraPathFromUsdRenderProductOverride: Render product invalid; "
                "no camera override.\n");

        return ufeCameraPath;
    }

    UsdRelationship productCameraRel = renderProduct.GetCameraRel();
    SdfPathVector   productCameraTargets;
    if (productCameraRel.GetTargets(&productCameraTargets)
        && !productCameraTargets.empty()) {
        const SdfPath usdCameraPath = productCameraTargets[0];
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Render product camera override target (%s): %s\n",
            renderProduct.GetPrim().GetPath().GetText(),
            usdCameraPath.GetText());
        UsdStageRefPtr  stage = renderProduct.GetPrim().GetStage();
        auto      stagePath = MayaUsdAPI::stagePath(stage);
        return 
            Ufe::Path::Segments { stagePath.getSegments()[0],
                                    MayaUsdAPI::usdPathToUfePathSegment(usdCameraPath) };
    } else {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No camera override on render product: %s\n",
            renderProduct.GetPrim().GetPath().GetText());
    }

    return ufeCameraPath;
}

// Check if render product overrides resolution and update inputParams if it does.
bool UpdateResolutionFromUsdRenderProduct(
    const UsdRenderProduct& renderProduct,
    BatchRenderer::InputParams& inputParams,
    const UsdTimeCode& timeCode)
{
    UsdAttribute productResolutionAttr = renderProduct.GetResolutionAttr();
    if (productResolutionAttr.HasAuthoredValue()) {
        GfVec2i productResolution;
        if (productResolutionAttr.Get(&productResolution, timeCode)) {
            if (productResolution[0] > 0 && productResolution[1] > 0) {
                inputParams.width = productResolution[0];
                inputParams.height = productResolution[1];
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Render product resolution override (%s): %d x %d at time %.3f\n",
                    renderProduct.GetPrim().GetPath().GetText(),
                    productResolution[0],
                    productResolution[1],
                    timeCode.GetValue());
                return true;
            }
        }
    }
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "No render product resolution override (%s) at time %.3f\n",
        renderProduct.GetPrim().GetPath().GetText(),
        timeCode.GetValue());
    return false;
}

// Extract resolution from USD render settings.
GfVec2i GetResolutionFromUsdRenderSettings(const UsdRenderSettings& usdRenderSettings)
{
    GfVec2i resolution(0, 0);
    if (!usdRenderSettings) {
        TF_WARN("GetResolutionFromUsdRenderSettings: UsdRenderSettings invalid; "
                "resolution remains 0.\n");
        return resolution;
    }
    
    // Try to get resolution from render settings.
    UsdAttribute resolutionAttr = usdRenderSettings.GetResolutionAttr();
    if (resolutionAttr.HasAuthoredValue()) {
        if (resolutionAttr.Get(&resolution)) {
            TF_DEBUG_MSG(
                MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                "Resolution from render settings (%s): %d x %d\n",
                usdRenderSettings.GetPrim().GetPath().GetText(),
                resolution[0],
                resolution[1]);
            if (resolution[0] > 0 && resolution[1] > 0) {
                return resolution;
            }
        }
    }
    
    // If resolution not found in render settings, try render products.
    if (resolution[0] <= 0 || resolution[1] <= 0) {
        UsdRelationship renderProductsRel = usdRenderSettings.GetProductsRel();
        SdfPathVector targets;
        if (renderProductsRel.GetTargets(&targets)) {
            for (const SdfPath& productPath : targets) {
                UsdPrim productPrim = usdRenderSettings.GetPrim().GetStage()->GetPrimAtPath(productPath);
                if (productPrim.IsValid()) {
                    UsdRenderProduct renderProduct(productPrim);
                    if (renderProduct) {
                        UsdAttribute productResolutionAttr = renderProduct.GetResolutionAttr();
                        if (productResolutionAttr.HasAuthoredValue()) {
                            GfVec2i productResolution;
                            if (productResolutionAttr.Get(&productResolution)) {
                                if (resolution[0] <= 0) {
                                    resolution[0] = productResolution[0];
                                }
                                if (resolution[1] <= 0) {
                                    resolution[1] = productResolution[1];
                                }
                                TF_DEBUG_MSG(
                                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                                    "Resolution from render product (%s): %d x %d\n",
                                    productPath.GetText(),
                                    resolution[0],
                                    resolution[1]);
                                if (resolution[0] > 0 && resolution[1] > 0) {
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return resolution;
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

} // namespace

namespace MAYAHYDRA_NS_DEF {

bool HydraRenderCmd::hydraRenderFromHydraV1RenderSettings()
{
    if (!_batchRenderer) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "_batchRenderer is a nullptr.\n");
        return false;
    }
    MStatus status;

    // Get the render settings from the scene
    UsdRenderSettings usdRenderSettings;
    const auto psPath = ExtractUsdRenderSettingsFromScene(usdRenderSettings);
    if (psPath.empty()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No USD render settings found in Maya USD proxy shapes.\n");
        return false;
    }
    BatchRenderer::InputParams inputParams;
    const GfVec2i renderSettingsResolution = GetResolutionFromUsdRenderSettings(usdRenderSettings);
    unsigned int  renderSettingsWidth = renderSettingsResolution[0]; // Store them to reuse them later
    unsigned int  renderSettingsHeight = renderSettingsResolution[1];
    inputParams.width   = renderSettingsWidth;
    inputParams.height  = renderSettingsHeight;
    TF_DEBUG_MSG(
        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
        "Initial render settings resolution: %u x %u\n",
        inputParams.width,
        inputParams.height);
    
    // Not considering render layers at all: both renderSetup and
    // legacy render layers are treated as legacy functionality.  Use
    // default render layer for all render products.
    MFnRenderLayer::defaultRenderLayer(&status);
    if (status != MS::kSuccess) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Failed to get default render layer.\n");
        return false;
    }

    // Resolve default camera once from render settings (used for products without overrides).
    const Ufe::Path defaultUfeCameraPath = GetUfeCameraPathFromUsdRenderSettings(usdRenderSettings);
    if (!defaultUfeCameraPath.empty()) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Default render-settings camera: %s\n",
            Ufe::PathString::string(defaultUfeCameraPath).c_str());
    } else {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No default render-settings camera resolved.\n");
    }

    auto* renderIndex = _batchRenderer->renderIndex();
    auto* renderDelegate = (renderIndex) ?renderIndex->GetRenderDelegate() : nullptr;
    if( ! renderDelegate ) {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Render delegate unavailable; cannot apply USD render settings.\n");
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

    // Loop over all render times.
    for (const MTime& time : renderTimes) {
        const double frameNb = time.as(MTime::uiUnit());
        if (MAnimControl::currentTime() != time) {
            MAnimControl::setCurrentTime(time);
        }
        const UsdTimeCode usdTimeCode(frameNb);

        ApplyUsdRenderSettingsToRenderDelegate(
            usdRenderSettings,
            usdTimeCode,
            renderDelegate);

        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "Rendering frame %.3f\n",
            frameNb);
        
        // Loop over all render products
        UsdRelationship renderProductsRel = usdRenderSettings.GetProductsRel();
        SdfPathVector productTargets;
        if (renderProductsRel.GetTargets(&productTargets)) {
            for (const SdfPath& productPath : productTargets) {
                UsdPrim productPrim
                    = usdRenderSettings.GetPrim().GetStage()->GetPrimAtPath(productPath);
                if (!productPrim.IsValid()) {
                    TF_WARN(
                        "hydraRenderFromHydraV1RenderSettings: Render product prim invalid "
                        "at path: %s\n",
                        productPath.GetText());
                    continue;
                }

                UsdRenderProduct renderProduct(productPrim);
                if (!renderProduct) {
                    TF_WARN(
                        "hydraRenderFromHydraV1RenderSettings: Render product schema invalid "
                        "at path: %s\n",
                        productPath.GetText());
                    continue;
                }

                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Processing render product: %s\n",
                    productPath.GetText());
                
                // Get product name (contains the image name pattern)
                TfToken     productNameToken;
                std::string imageName;
                const char* imageNameSource = "render product";
                if (renderProduct.GetProductNameAttr().Get(&productNameToken, usdTimeCode)) {
                    imageName = productNameToken.GetString();
                }
                if (imageName.empty()) {
                    TfToken settingsProductNameToken;
                    UsdAttribute settingsProductNameAttr
                        = usdRenderSettings.GetPrim().GetAttribute(UsdRenderTokens->productName);
                    if (settingsProductNameAttr
                        && settingsProductNameAttr.Get(&settingsProductNameToken, usdTimeCode)) {
                        imageName = settingsProductNameToken.GetString();
                        imageNameSource = "render settings";
                    }
                }
                if (imageName.empty()) {
                    imageName = productPrim.GetName().GetString();
                    imageNameSource = "product prim name";
                }
                if (imageName.empty()) {
                    imageName = "render";
                    imageNameSource = "fallback";
                }
                if (std::filesystem::path(imageName).extension().empty()) {
                    imageName += ".exr";
                }
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Render product name for %s: %s (source=%s)\n",
                    productPath.GetText(),
                    imageName.c_str(),
                    imageNameSource);

                // Extract render vars (AOVs) from the render product
                const RenderVarsInfo renderVarsInfo
                    = GetRenderVarsFromUsdRenderProduct(renderProduct);
                inputParams.renderVarsInfo = renderVarsInfo;
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Render vars count for %s: %zu\n",
                    productPath.GetText(),
                    renderVarsInfo.renderVars.size());
                
                // Get UFE camera path: default from render settings, override per render product if present
                Ufe::Path ufeCameraPath = defaultUfeCameraPath;
                const Ufe::Path productOverrideCameraPath =
                    GetUfeCameraPathFromUsdRenderProductOverride(renderProduct);
                if (!productOverrideCameraPath.empty()) {
                    ufeCameraPath = productOverrideCameraPath;
                    TF_DEBUG_MSG(
                        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                        "Using product camera override: %s\n",
                        Ufe::PathString::string(ufeCameraPath).c_str());
                }
                
                // If no camera found, skip this product
                if (ufeCameraPath.empty()) {
                    TF_DEBUG_MSG(
                        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                        "No camera resolved for product %s; skipping.\n",
                        productPath.GetText());
                    continue;
                }
                
                inputParams.ufeCameraPath = ufeCameraPath;

                // Reset to render settings resolution before applying product override.
                inputParams.width   = renderSettingsWidth;
                inputParams.height  = renderSettingsHeight;
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Base resolution for product %s: %u x %u\n",
                    productPath.GetText(),
                    inputParams.width,
                    inputParams.height);

                // Check if render product overrides resolution
                UpdateResolutionFromUsdRenderProduct(renderProduct, inputParams, usdTimeCode);

                // Use the image name from the render product (replace frame number if needed)
                // The image name pattern may contain frame placeholders that need to be resolved
                std::string baseImageNameStr = imageName;

                // Replace frame number placeholder in the image name if present
                // Maya typically uses <f> or <f4> etc. for frame numbers
                std::string frameStr = std::to_string(static_cast<int>(frameNb));
                // Simple replacement - in practice you might need more sophisticated pattern
                // matching
                size_t pos = baseImageNameStr.find("<f>");
                if (pos != std::string::npos) {
                    baseImageNameStr.replace(pos, 3, frameStr);
                }
                pos = baseImageNameStr.find("<f4>");
                if (pos != std::string::npos) {
                    std::string frameStr4 = std::to_string(static_cast<int>(frameNb));
                    while (frameStr4.length() < 4) {
                        frameStr4 = "0" + frameStr4;
                    }
                    baseImageNameStr.replace(pos, 4, frameStr4);
                }

                const std::string resolvedImageName = ResolveRenderProductImagePath(
                    baseImageNameStr,
                    renderProduct.GetPrim().GetStage());
                if (resolvedImageName != baseImageNameStr) {
                    TF_DEBUG_MSG(
                        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                        "Resolved render product path: %s -> %s\n",
                        baseImageNameStr.c_str(),
                        resolvedImageName.c_str());
                }

                auto     resetFileName = []() { Fvp::ImageBufferWriter::SetFileName(""); };
                TfScoped guard(resetFileName);
                    
                const std::filesystem::path outputPath(resolvedImageName);
                const std::filesystem::path outputDir = outputPath.parent_path();
                if (!outputDir.empty()) {
                    std::error_code ec;
                    std::filesystem::create_directories(outputDir, ec);
                    if (ec) {
                        TF_DEBUG_MSG(
                            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                            "Failed to create output directory %s: %s\n",
                            outputDir.string().c_str(),
                            ec.message().c_str());
                    }
                }
                MString finalImageName(resolvedImageName.c_str());

                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Render image name is %s.\n",
                    finalImageName.asChar());

                // Set the filename in the ImageBufferWriter.
                Fvp::ImageBufferWriter::SetFileName(finalImageName.asChar());

                if (_batchRenderer->RenderFromHydraV1RenderSettings(inputParams) != MS::kSuccess) {
                    TF_DEBUG_MSG(
                        MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                        "BatchRenderer::Render failed for product %s.\n",
                        productPath.GetText());
                    return false;
                }
            } // Render product loop
        }//if (renderProductsRel.GetTargets(&productTargets)) {
    } // Time loop

    return true;
}

} // namespace MAYAHYDRA_NS_DEF
