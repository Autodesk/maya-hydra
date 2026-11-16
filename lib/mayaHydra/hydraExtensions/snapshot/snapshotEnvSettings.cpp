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
// Environment flag for the frame publication boundary.

#include "snapshotEnvSettings.h"

#include <pxr/base/tf/envSetting.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(
    MAYAHYDRA_SNAPSHOT,
    false,
    "Enable the frame publication boundary: advance a frame id per flush and "
    "publish it once the frame is complete.");

TF_DEFINE_ENV_SETTING(
    MAYAHYDRA_PIPELINE_OVERLAP,
    false,
    "Render on a separate thread, overlapping the draw of one frame with the "
    "main thread's work on the next. Implies MAYAHYDRA_SNAPSHOT.");

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

bool IsSnapshotEnabled()
{
    // Overlap implies the boundary: the render thread identifies the frame it is drawing
    // by the published frame id, and would otherwise read 0 for every frame.
    static const bool _enabled = PXR_NS::TfGetEnvSetting(PXR_NS::MAYAHYDRA_SNAPSHOT)
        || PXR_NS::TfGetEnvSetting(PXR_NS::MAYAHYDRA_PIPELINE_OVERLAP);
    return _enabled;
}

bool IsPipelineOverlapEnabled()
{
    static const bool _enabled = PXR_NS::TfGetEnvSetting(PXR_NS::MAYAHYDRA_PIPELINE_OVERLAP);
    return _enabled;
}

} // namespace MAYAHYDRA_NS_DEF
