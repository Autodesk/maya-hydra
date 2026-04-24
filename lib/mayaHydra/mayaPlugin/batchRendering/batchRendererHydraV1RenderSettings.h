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

#ifndef MAYAHYDRA_BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H
#define MAYAHYDRA_BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H

#include "batchRenderer.h"

namespace MAYAHYDRA_NS_DEF {

/*! \brief Batch rendering strategy using Hydra V1 render settings.
 *
 *  The batch renderer reads USD render settings prims (UsdRenderSettings,
 *  UsdRenderProduct, UsdRenderVar) from a USD stage in the scene, extracts
 *  resolution, camera, AOVs, and render products, and applies them to the
 *  Hydra task controller.  The batch renderer manages the render loop,
 *  convergence, and image output.
 */
class BatchRendererHydraV1RenderSettings
{
public:
    /// Perform a batch render using Hydra V1 render settings.
    static MStatus Render(
        BatchRenderer& renderer,
        const BatchRenderer::InputParams& inputParams);
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H
