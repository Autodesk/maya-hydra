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

#include <ufe/ufe.h>

#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/usd/stage.h>
#include <pxr/usd/usdRender/settings.h>

#include <string>
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
 *  MayaHydra supports two render settings strategies:
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
    HydraV1,
    HydraV2,
};

/// Determine the RenderSettingsType from the render delegate.
RenderSettingsType ReadRenderSettingsTypeFromRenderDelegate(const PXR_NS::TfToken& rendererName);

// Extract the UsdRenderSettings named by the active render description path.
// Returns the two-segment UFE path (proxy shape, then render settings prim) of
// the active render settings prim, or an empty path when it does not resolve to
// a UsdRenderSettings prim with at least one render product.
Ufe::Path ExtractUsdRenderSettingsFromScene(PXR_NS::UsdRenderSettings& usdRenderSettings);

// Get the UFE application path to the active render settings prim from the Maya scene.
Ufe::Path GetActiveRenderSettingsAppPath();

// Get the Hydra path to the active render settings prim from the Maya scene.
PXR_NS::SdfPath GetActiveRenderSettingsHydraPath();

// Get the UFE application path to the active render pass prim from the Maya scene.
Ufe::Path GetActiveRenderPassAppPath();

// Get the Hydra path to the active render pass prim from the Maya scene.
PXR_NS::SdfPath GetActiveRenderPassHydraPath();

// Get render output tokens from the active Hydra render settings prim.
PXR_NS::TfTokenVector GetRenderOutputsFromActiveRenderSettings(
    const PXR_NS::HdRenderIndex* renderIndex);

struct RenderTimes
{
    /// Inclusive frame range, in Maya UI time units.
    struct TimeRange
    {
        MTime startTime;
        MTime endTime;
    };

    const std::vector<TimeRange> timeRanges;
    /// Frame increment, in frames.
    const float timeIncr;

    RenderTimes(std::vector<TimeRange> timeRanges, float timeIncr);

    /// Total number of frames over all ranges, with the increment applied.
    int FrameCount() const;

    /// The frames to render, in range order, with the increment applied.
    std::vector<MTime> FrameTimes() const;
};

// Get the render times from the Maya scene.
RenderTimes GetRenderTimes();

// Single-line description of the render times, for debug output.
std::string RenderTimesDescription(const RenderTimes& renderTimes);

// Report to Maya the progress of a batch render, as an integer percentage of
// the frames in renderTimes, given the number of frames rendered so far.
void SendRenderProgress(const RenderTimes& renderTimes, int framesDone);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_SETTINGS_UTILS_H
