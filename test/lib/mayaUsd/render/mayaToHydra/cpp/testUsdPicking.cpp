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

#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/observableSelection.h>
#include <ufe/globalSelection.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestUsdPicking, pick)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path selected(Ufe::PathString::path(argv[0]));

    // Maya selection API doesn't understand USD data, which can only be
    // represented through UFE, so use UFE API to interact with Maya selection.
    const auto sn = Ufe::GlobalSelection::get();
    sn->clear();

    // Translate the application path into a scene index path using the
    // selection scene index.
    // The Flow Viewport selection scene index is in the scene index tree.
    const auto snSi = findSelectionSceneIndexInTree(siRoot);
    ASSERT_TRUE(snSi);

    const auto primSelections = snSi->ConvertUfeSelectionToHydra(selected);
    ASSERT_EQ(primSelections.size(), 1u);
    const auto sceneIndexPath = primSelections.front().primPath;

    ASSERT_FALSE(sceneIndexPath.IsEmpty());

    const auto prim = siRoot->GetPrim(sceneIndexPath);
    ASSERT_TRUE(prim.dataSource);

    // There is no selections data source on the prim.
    auto dataSourceNames = prim.dataSource->GetNames();
    ASSERT_EQ(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());

    // Selection scene index says the prim is not selected.
    ASSERT_FALSE(snSi->IsFullySelected(sceneIndexPath));

    //======================================================================
    // Perform a pick
    //======================================================================

    M3dView active3dView = M3dView::active3dView();

    const auto primMouseCoords = getPrimMouseCoords(prim, active3dView);

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

    //======================================================================
    // Test that the pick changed the Maya selection
    //======================================================================

    // When picking on the boundary of multiple objects, one Hydra pick hit per
    // object is returned.  Therefore test that the expected selected path is
    // in the selection.
    ASSERT_GE(sn->size(), 1u);
    ASSERT_TRUE(sn->contains(selected));

    //======================================================================
    // Test that the pick changed the Hydra selection
    //======================================================================

    // On selection, the prim is given a selections data source.
    dataSourceNames = prim.dataSource->GetNames();
    ASSERT_NE(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());

    auto snDataSource = prim.dataSource->Get(HdSelectionsSchemaTokens->selections);
    ASSERT_TRUE(snDataSource);
    auto selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    ASSERT_TRUE(selectionsSchema);

    // Only one selection in the selections schema.
    ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
    auto selectionSchema = selectionsSchema.GetElement(0);

    // Prim is fully selected.
    auto ds = selectionSchema.GetFullySelected();
    ASSERT_TRUE(ds);
    ASSERT_TRUE(ds->GetTypedValue(0.0f));

    // Selection scene index says the prim is selected.
    ASSERT_TRUE(snSi->IsFullySelected(sceneIndexPath));
    ASSERT_TRUE(snSi->HasFullySelectedAncestorInclusive(sceneIndexPath));
}
