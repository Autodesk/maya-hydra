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

#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/observableSelection.h>
#include <ufe/globalSelection.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestUsdPointInstancePicking, pickPointInstance)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const Ufe::Path selected(Ufe::PathString::path(argv[0]));
    const Ufe::Path marker(Ufe::PathString::path(argv[1]));

    // Maya selection API doesn't understand USD data, which can only be
    // represented through UFE, so use UFE API to interact with Maya selection.
    const auto sn = Ufe::GlobalSelection::get();
    sn->clear();

    // Translate the application path into a scene index path using the
    // selection scene index.
    // The Flow Viewport selection scene index is in the scene index tree.
    const auto selectionSi = findSelectionSceneIndexInTree(siRoot);
    ASSERT_TRUE(selectionSi);

    const auto sceneIndexPath = selectionSi->SceneIndexPath(marker);

    ASSERT_FALSE(sceneIndexPath.IsEmpty());

    const auto markerPrim = siRoot->GetPrim(sceneIndexPath);
    ASSERT_TRUE(markerPrim.dataSource);

    M3dView active3dView = M3dView::active3dView();

    const auto primMouseCoords = getPrimMouseCoords(markerPrim, active3dView);

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

    // When picking on the boundary of multiple objects, one Hydra pick hit per
    // object is returned.  Therefore test that the expected selected path is
    // in the selection.
    ASSERT_GE(sn->size(), 1u);
    ASSERT_TRUE(sn->contains(selected));
}
