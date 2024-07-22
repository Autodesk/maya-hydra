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

#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <ufe/path.h>
#include <ufe/pathString.h>
#include <ufe/observableSelection.h>
#include <ufe/globalSelection.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

// Check for correspondence between Maya selection and Hydra scene index
// selection.
TEST(TestSelection, fullySelectedPaths)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto siRoot = sceneIndices.front();

    // Translate the application path into a scene index path using the
    // selection scene index.
    // The Flow Viewport selection scene index is in the scene index tree.
    const auto snSi = findSelectionSceneIndexInTree(siRoot);
    ASSERT_TRUE(snSi);

    if (testingArgsEmpty()) {
        // Check for empty scene index selection.
        ASSERT_TRUE(snSi->GetFullySelectedPaths().empty());
        return;
    }

    // Non-empty selection.
    auto [argc, argv] = getTestingArgs();

    const Ufe::Path selected(Ufe::PathString::path(argv[0]));

    const auto sceneIndexPath = snSi->SceneIndexPath(selected);

    ASSERT_FALSE(sceneIndexPath.IsEmpty());

    const auto prim = siRoot->GetPrim(sceneIndexPath);
    ASSERT_TRUE(prim.dataSource);

    // On selection, the prim is given a selections data source.
    auto dataSourceNames = prim.dataSource->GetNames();
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
    auto fullySelectedPaths = snSi->GetFullySelectedPaths();
    ASSERT_EQ(fullySelectedPaths.size(), 1u);
    ASSERT_NE(std::find(fullySelectedPaths.cbegin(), fullySelectedPaths.cend(),
                        sceneIndexPath), fullySelectedPaths.cend());
}
