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

#include "batchRendererHydraV2RenderSettings.h"

#include "pluginDebugCodes.h"

#include <mayaHydraLib/sceneIndex/registration.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hdx/renderTask.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MStatus BatchRendererHydraV2RenderSettings::Render(BatchRenderer& renderer)
{
    // Hydra V2 render settings path: render delegate owns render-settings logic.
    TF_DEBUG(MAYAHYDRALIB_RENDEROVERRIDE_RENDER)
        .Msg("BatchRenderer::RenderFromMayaRenderSettings()\n");

    // Use dummy values; the render delegate reads render settings directly in Hydra.
    constexpr int width   = 640;
    constexpr int  height = 480;

    HdxRenderTaskParams params;
    if (!renderer._PrepareHydraBatchRender(width, height, &params)) {
        return MStatus::kFailure;
    }

    renderer._FinalizeHydraBatchRender(params);

    // The common render frame does too much to be called in a loop:
    // all we want is to call it once, then call _Execute() repeatedly.
    renderer._ExecuteHydraBatchRenderFrame();

    return MStatus::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
