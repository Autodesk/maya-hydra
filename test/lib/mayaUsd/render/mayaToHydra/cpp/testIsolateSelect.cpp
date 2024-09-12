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
#include <flowViewport/selection/fvpPathMapperRegistry.h>
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

    auto primSelections = Fvp::ufePathToPrimSelections(appPath);

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

    auto primSelections = Fvp::ufePathToPrimSelections(appPath);

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

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    auto primSelections = Fvp::ufePathToPrimSelections(appPath);
    vpDataMgr.AddIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, remove)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string viewportId(argv[0]);
    const Ufe::Path appPath(Ufe::PathString::path(argv[1]));

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    auto primSelections = Fvp::ufePathToPrimSelections(appPath);
    vpDataMgr.RemoveIsolateSelection(viewportId, primSelections);
}

TEST(TestIsolateSelection, clear)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string viewportId(argv[0]);

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.ClearIsolateSelection(viewportId);
}

TEST(TestIsolateSelection, replace)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 2);
    const std::string viewportId(argv[0]);
    
    // Use allocator from Flow Viewport Toolkit library.  Directly calling
    //
    // auto isolateSelect = std::make_shared<Fvp::Selection>();
    //
    // here will cause a crash on test exit, as the last reference to the
    // isolate selection is in the isolate select scene index, which can be
    // destroyed and rebuilt on redraw.  On Windows, if the DLL that performed
    // the allocation of the shared_ptr has been unloaded from the process, the
    // isolate selection destruction called by the shared_ptr destructor will
    // crash.
    //
    // This is exactly our case, as our host mayaHydraCppTests DLL plugin is
    // unloaded at program exit, but Maya performs additional redraws after the
    // plugin unload, which cause the isolate selection shared_ptr to be
    // destroyed, which causes a call to code in an unloaded DLL, which crashes.
    auto isolateSelect = Fvp::Selection::New();

    for (int i=1; i < argc; ++i) {
        const Ufe::Path appPath(Ufe::PathString::path(argv[i]));
        auto primSelections = Fvp::ufePathToPrimSelections(appPath);
        for (const auto& primSelection : primSelections) {
            isolateSelect->Add(primSelection);
        }
    }

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.ReplaceIsolateSelection(viewportId, isolateSelect);
}

TEST(TestIsolateSelection, disable)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const std::string viewportId(argv[0]);

    auto& vpDataMgr = Fvp::ViewportDataMgr::Get();
    vpDataMgr.DisableIsolateSelection(viewportId);
}

