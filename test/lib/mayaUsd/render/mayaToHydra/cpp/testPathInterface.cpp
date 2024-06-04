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

#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

#include <iostream>

using namespace MAYAHYDRA_NS;

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

SdfPath getArgSceneIndexPath(const Fvp::SelectionSceneIndexRefPtr& snSi)
{
    // Object path string is in command line arguments.
    auto [argc, argv] = getTestingArgs();

    // Get the application data model path to the selected object.
    const auto mayaPath = Ufe::PathString::path(argv[0]);

    // Translate the application path into a scene index path.
    auto primSelections = snSi->ConvertUfePathToHydraSelections(mayaPath);
    EXPECT_GE(primSelections.size(), 1u);
    return primSelections.empty() ? SdfPath() : primSelections.front().primPath;
}

Fvp::SelectionSceneIndexRefPtr getSelectionSceneIndex()
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto isFvpSelectionSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Selection Scene Index");
    auto selectionSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpSelectionSceneIndex);
    return TfDynamic_cast<Fvp::SelectionSceneIndexRefPtr>(selectionSiBase);
}

}

TEST(TestPathInterface, testSceneIndices)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto childPrims = sceneIndices.front()->GetChildPrimPaths(SdfPath("/MayaUsdProxyShape_PluginNode"));
    ASSERT_EQ(childPrims.size(), 3u);
}

TEST(TestPathInterface, testSelected)
{
    // Get the Flow Viewport selection scene index.
    auto snSi = getSelectionSceneIndex();
    
    // Selected object path string is in command line arguments.
    // Get it and translate it into a scene index path.
    const auto sceneIndexPath = getArgSceneIndexPath(snSi);

    // Confirm the object is selected in scene index scene.
    ASSERT_TRUE(snSi->IsFullySelected(sceneIndexPath));
}

TEST(TestPathInterface, testUnselected)
{
    // Get the Flow Viewport selection scene index.
    auto snSi = getSelectionSceneIndex();
    
    // Unselected object path string is in command line arguments.
    // Get it and translate it into a scene index path.
    const auto sceneIndexPath = getArgSceneIndexPath(snSi);

    // Confirm the object is not selected in scene index scene.
    ASSERT_FALSE(snSi->IsFullySelected(sceneIndexPath));
}
