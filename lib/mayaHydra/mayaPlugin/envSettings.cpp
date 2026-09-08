//
// Copyright 2025 Autodesk
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

#include "envSettings.h"
#include <pxr/base/tf/envSetting.h>

PXR_NAMESPACE_OPEN_SCOPE
// Define the environment setting in the PXR namespace
TF_DEFINE_ENV_SETTING(
    MAYA_HYDRA_SINGLE_FRAME_PASS,
    false,
    "Use single frame pass when using the same renderer for all passes.");

TF_DEFINE_ENV_SETTING(
    MAYA_HYDRA_USE_CAMERA_PRIM,
    false,
    "Pass camera prim to Hydra instead of populating the free camera parameters."
    "Camera prim may expose additional render parameters but its usage must be well tested before we can enable it by default.");

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {
    bool useSingleFramePass()
    {
        // Check the environment variable to determine if we should use a single frame pass
        static const bool _useSingleFramePass
            = PXR_NS::TfGetEnvSetting(PXR_NS::MAYA_HYDRA_SINGLE_FRAME_PASS);
        return _useSingleFramePass;
    }

    bool useCameraPrim()
    {
        static const bool _useCameraPrim
            = PXR_NS::TfGetEnvSetting(PXR_NS::MAYA_HYDRA_USE_CAMERA_PRIM);
        return _useCameraPrim;
    }

} // namespace MAYAHYDRA_NS_DEF
