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

#include <pxr/pxr.h>
#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

bool HydraRenderCmd::hydraRenderFromHydraV2RenderSettings()
{
    if (!_batchRenderer) {
        TF_WARN("hydraRenderFromHydraV2RenderSettings: _batchRenderer is a nullptr.\n");
        return false;
    }

    return _batchRenderer->RenderFromHydraV2RenderSettings();
}

} // namespace MAYAHYDRA_NS_DEF
