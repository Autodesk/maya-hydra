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

#ifndef MAYAHYDRA_ENV_SETTINGS_H
#define MAYAHYDRA_ENV_SETTINGS_H

#include <mayaHydraLib/api.h>

namespace MAYAHYDRA_NS_DEF {
    bool useSingleFramePass();

    /// Whether viewport hover highlighting is enabled (default true).
    /// Set MAYA_HYDRA_ENABLE_HOVER=0 to disable the viewport hover feature
    /// entirely (no mouse tracking, no hover-driven refreshes, no hover picks).
    bool enableHover();
}

#endif // MAYAHYDRA_ENV_SETTINGS_H
