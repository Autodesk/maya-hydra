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

#ifndef MAYAHYDRA_RENDER_SETTINGS_UTILS_H
#define MAYAHYDRA_RENDER_SETTINGS_UTILS_H

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MTime.h>

#include <ufe/sceneItemList.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdRender/settings.h>

#include <vector>

namespace UFE_VERSIONED_NS {
class Path;
}

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/*! \brief Determines how render settings are sourced for batch rendering.
 *
 *  MayaHydra supports three render settings strategies:
 *
 *  - **Maya**: Traditional Maya render settings (defaultRenderGlobals,
 *    defaultResolution, etc.) are read and translated into Hydra task
 *    controller parameters.  The batch renderer manages the render loop,
 *    convergence, and image output.
 *
 *  - **HydraV1**: The batch renderer reads USD render settings prims
 *    (UsdRenderSettings, UsdRenderProduct, UsdRenderVar) from a USD
 *    stage in the scene, extracts resolution, camera, AOVs, and render
 *    products, and applies them to the Hydra task controller.  The batch
 *    renderer still manages the render loop, convergence, and image
 *    output.
 *
 *  - **HydraV2**: The render delegate itself reads the USD render
 *    settings prims directly from the Hydra scene and drives the render
 *    pass internally.  The batch renderer only provides the execution
 *    environment; configuration and output are handled entirely by the
 *    render delegate (e.g. Hydra Prman with
 *    HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS enabled).
 *
 *  The strategy is selected automatically based on the render delegate
 *  capabilities and the presence of USD render settings in the scene.
 *  See ReadRenderSettingsTypeFromRenderDelegate().
 */
enum class RenderSettingsType
{
    Unknown = 0,
    Maya,
    HydraV1,
    HydraV2,
};

/// Determine the RenderSettingsType from the render delegate.
RenderSettingsType ReadRenderSettingsTypeFromRenderDelegate(const PXR_NS::TfToken& rendererName);

Ufe::SceneItemList GetAllMayaUsdProxyShapes();

// Find UsdRenderSettings on the given stage.
bool FindUsdRenderSettingsOnStage(
    const PXR_NS::UsdStageRefPtr& stage,
    PXR_NS::UsdRenderSettings&    outSettings);

// Extract UsdRenderSettings from all MayaUsdProxyShapes in the scene.
// Returns the path to the proxy shape node if found, empty path if not.
Ufe::Path ExtractUsdRenderSettingsFromScene(PXR_NS::UsdRenderSettings& usdRenderSettings);

// Get the path to the active Hydra render settings prim from the Maya scene.
PXR_NS::SdfPath GetActiveRenderSettingsPrimHydraPathFromScene();

// Get render output tokens from the active Hydra render settings prim.
PXR_NS::TfTokenVector GetRenderOutputsFromActiveRenderSettings(
    const PXR_NS::HdRenderIndex* renderIndex);

// Get render times from the USD stage time range.
std::vector<MTime> GetRenderTimesFromStage(const PXR_NS::UsdStageRefPtr& stage);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_SETTINGS_UTILS_H
