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

#ifndef BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H
#define BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H

#include "batchRenderer.h"

namespace MAYAHYDRA_NS_DEF {

class BatchRendererHydraV1RenderSettings
{
public:
    static MStatus Render(
        BatchRenderer& renderer,
        const BatchRenderer::InputParams& inputParams);
};

} // namespace MAYAHYDRA_NS_DEF

#endif // BATCH_RENDERER_HYDRA_V1_RENDER_SETTINGS_H
