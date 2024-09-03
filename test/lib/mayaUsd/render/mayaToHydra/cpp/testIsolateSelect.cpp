// Copyright 2024 Autodesk
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

#include "testUtils.h"

#include <mayaHydraLib/hydraUtils.h>

#include <flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h>
#include <flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h>
#include <flowViewport/sceneIndex/fvpPathInterface.h>
#include <flowViewport/selection/fvpSelection.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestHydraPrim, isVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    auto primSelections = ufePathToPrimSelections(appPath);

    // If an application path maps to multiple prim selections, all prim
    // selections must be visible, else we fail.
    unsigned int primVis = 0;
    for (const auto& primSelection : primSelections) {
        if (visibility(siRoot, primSelection.primPath)) {
            ++primVis;
        }
    }

    ASSERT_EQ(primVis, primSelections.size());
}

TEST(TestHydraPrim, notVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    auto primSelections = ufePathToPrimSelections(appPath);

    // If an application path maps to multiple prim selections, all prim
    // selections must be invisible, else we fail.
    int primVis = 0;
    for (const auto& primSelection : primSelections) {
        if (visibility(siRoot, primSelection.primPath)) {
            ++primVis;
        }
    }

    ASSERT_EQ(primVis, 0);
}

TEST(TestIsolateSelection, add)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string viewportId(argv[0]);
    const Ufe::Path appPath(Ufe::PathString::path(argv[1]));

    auto& perVpDataMgr = Fvp::PerViewportDataManager::Get();
    auto primSelections = ufePathToPrimSelections(appPath);
    perVpDataMgr.AddIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, remove)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string viewportId(argv[0]);
    const Ufe::Path appPath(Ufe::PathString::path(argv[1]));

    auto& perVpDataMgr = Fvp::PerViewportDataManager::Get();
    auto primSelections = ufePathToPrimSelections(appPath);
    perVpDataMgr.RemoveIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, clear)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string viewportId(argv[0]);

    auto& perVpDataMgr = Fvp::PerViewportDataManager::Get();
    perVpDataMgr.ClearIsolateSelection(viewportId);
}

TEST(TestIsolateSelection, replace)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 2);
    const std::string viewportId(argv[0]);
    
    auto isolateSelect = std::make_shared<Fvp::Selection>();

    for (int i=1; i < argc; ++i) {
        const Ufe::Path appPath(Ufe::PathString::path(argv[i]));
        auto primSelections = ufePathToPrimSelections(appPath);
        for (const auto& primSelection : primSelections) {
            isolateSelect->Add(primSelection);
        }
    }

    auto& perVpDataMgr = Fvp::PerViewportDataManager::Get();
    perVpDataMgr.ReplaceIsolateSelection(viewportId, isolateSelect);
}
