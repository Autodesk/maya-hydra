// Copyright 2023 Autodesk
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

#ifndef ADSK_HYDRA_SCENE_BROWSER_TESTING_H
#define ADSK_HYDRA_SCENE_BROWSER_TESTING_H

#include "adskHydraSceneBrowserTestApi.h"

#include <pxr/imaging/hd/sceneIndex.h>

namespace AdskHydraSceneBrowserTesting {
HDUITEST_API
bool RunFullSceneIndexComparisonTest(pxr::HdSceneIndexBasePtr referenceSceneIndex);

HDUITEST_API
bool RunSceneCorrectnessTest(pxr::HdSceneIndexBasePtr referenceSceneIndex);
} // namespace AdskHydraSceneBrowserTesting

#endif // ADSK_HYDRA_SCENE_BROWSER_TESTING_H
