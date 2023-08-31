
#ifndef ADSK_HYDRA_SCENE_BROWSER_TESTING_H
#define ADSK_HYDRA_SCENE_BROWSER_TESTING_H

#include "adskHydraSceneBrowserTestApi.h"

#include <pxr/imaging/hd/sceneIndex.h>

namespace AdskHydraSceneBrowserTesting {
HDUITEST_API
bool RunFullSceneIndexComparisonTest(pxr::HdSceneIndexBasePtr referenceSceneIndex);
} // namespace AdskHydraSceneBrowserTesting

#endif // ADSK_HYDRA_SCENE_BROWSER_TESTING_H
