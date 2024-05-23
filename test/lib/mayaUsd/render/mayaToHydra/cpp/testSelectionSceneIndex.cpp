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

#include "testUtils.h"

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>

#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>

#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hdx/selectionSceneIndexObserver.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

TEST(FlowViewport, selectionSceneIndex)
{
    // The Flow Viewport selection scene index is in the scene index tree.
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto isFvpSelectionSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Selection Scene Index");
    auto selectionSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpSelectionSceneIndex);
    ASSERT_TRUE(selectionSiBase);

    auto selectionSi = TfDynamic_cast<Fvp::SelectionSceneIndexRefPtr>(selectionSiBase);
    ASSERT_TRUE(selectionSi);
    
    // Get the path to the sphere defined in the Python driver for this test.
    // We know this is a single-segment UFE path, and that its tail is
    // "aSphere".
    const auto mayaPath = Ufe::PathString::path("|aSphere");
    ASSERT_EQ(mayaPath.nbSegments(), 1u);
    ASSERT_EQ(mayaPath.back().string(), "aSphere");

    // Clear the Maya selection.  Can be done equivalently either through the
    // Maya MSelectionList interface or the UFE selection interface.
    ASSERT_EQ(MGlobal::clearSelectionList(), MS::kSuccess);

    // The sphere prim in the Hydra scene index scene has no selection data
    // source.  First, translate the application path into a scene index path.
    const auto sceneIndexPath = selectionSi->ConvertUfeSelectionToHydra(mayaPath).front().primPath;
    ASSERT_EQ(sceneIndexPath.GetName(), mayaPath.back().string());

    // Next, check that there is no selections data source on the prim.
    auto spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    ASSERT_TRUE(spherePrim.dataSource);
    auto dataSourceNames = spherePrim.dataSource->GetNames();
    ASSERT_EQ(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());

    // Selection scene index says the prim is not selected.
    ASSERT_FALSE(selectionSi->IsFullySelected(sceneIndexPath));

    // On selection, the prim is given a selections data source.
    MSelectionList sphereSn;
    ASSERT_EQ(sphereSn.add("|aSphere"), MS::kSuccess);
    ASSERT_EQ(MGlobal::setActiveSelectionList(sphereSn), MS::kSuccess);

    spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    ASSERT_TRUE(spherePrim.dataSource);
    dataSourceNames = spherePrim.dataSource->GetNames();
    ASSERT_NE(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());

    auto snDataSource = spherePrim.dataSource->Get(HdSelectionsSchemaTokens->selections);
    ASSERT_TRUE(snDataSource);
    auto selectionsSchema = HdSelectionsSchema::GetFromParent(spherePrim.dataSource);
    ASSERT_TRUE(selectionsSchema);

    // Only one selection in the selections schema.
    ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
    auto selectionSchema = selectionsSchema.GetElement(0);

    // Prim is fully selected.
    auto ds = selectionSchema.GetFullySelected();
    ASSERT_TRUE(ds);
    ASSERT_TRUE(ds->GetTypedValue(0.0f));

    // Selection scene index says the prim is selected.
    ASSERT_TRUE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_TRUE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));

    // The shape under the sphere transform is not selected, but it has a
    // selected ancestor.
    auto mayaShapePath = Ufe::PathString::path("|aSphere|aSphereShape");
    const auto sceneIndexShapePath = selectionSi->ConvertUfeSelectionToHydra(mayaShapePath).front().primPath;

    auto sphereShapePrim = sceneIndices.front()->GetPrim(sceneIndexShapePath);
    ASSERT_TRUE(sphereShapePrim.dataSource);
    dataSourceNames = sphereShapePrim.dataSource->GetNames();
    ASSERT_EQ(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());
    ASSERT_FALSE(selectionSi->IsFullySelected(sceneIndexShapePath));
    // HYDRA-626: cannot check for selected ancestor, as shape is in the
    // "Lighted" hierarchy, and its selected parent transform is not.
    // ASSERT_TRUE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexShapePath));

    // Remove the sphere from the selection: no longer a selections data source.
    ASSERT_EQ(MGlobal::clearSelectionList(), MS::kSuccess);

    spherePrim = sceneIndices.front()->GetPrim(sceneIndexPath);
    ASSERT_TRUE(spherePrim.dataSource);
    dataSourceNames = spherePrim.dataSource->GetNames();
    ASSERT_EQ(std::find(dataSourceNames.begin(), dataSourceNames.end(), HdSelectionsSchemaTokens->selections), dataSourceNames.end());
    ASSERT_FALSE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_FALSE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));
}

TEST(FlowViewport, selectionSceneIndexDirty)
{
    // The Flow Viewport selection scene index is in the scene index tree.
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto isFvpSelectionSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Selection Scene Index");
    auto selectionSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpSelectionSceneIndex);
    auto selectionSi = TfDynamic_cast<Fvp::SelectionSceneIndexRefPtr>(selectionSiBase);

    // The selection scene index observer builds its selection by tracking dirty
    // notifications on HdSelectionsSchema::GetDefaultLocator().  Use it to
    // ensure dirty notifications are correct.
    HdxSelectionSceneIndexObserver ssio;
    ssio.SetSceneIndex(selectionSiBase);

    // Clear the Maya selection.
    MGlobal::clearSelectionList();

    // Selection scene index observer should report an empty selection.
    auto hdSn = ssio.GetSelection();
    ASSERT_TRUE(hdSn->GetAllSelectedPrimPaths().empty());

    // Select the sphere
    MSelectionList sphereSn;
    sphereSn.add("|aSphere");
    const auto mayaPath = Ufe::PathString::path("|aSphere");
    const auto sceneIndexPath = selectionSi->ConvertUfeSelectionToHydra(mayaPath).front().primPath;

    MGlobal::setActiveSelectionList(sphereSn);
    hdSn = ssio.GetSelection();
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths().size(), 1u);
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths()[0], sceneIndexPath);
    ASSERT_TRUE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_TRUE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));

    // Remove the sphere from the selection.
    MGlobal::clearSelectionList();
    hdSn = ssio.GetSelection();
    ASSERT_TRUE(hdSn->GetAllSelectedPrimPaths().empty());
    ASSERT_FALSE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_FALSE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));

    // Add it back.
    MGlobal::setActiveSelectionList(sphereSn);
    hdSn = ssio.GetSelection();
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths().size(), 1u);
    ASSERT_EQ(hdSn->GetAllSelectedPrimPaths()[0], sceneIndexPath);
    ASSERT_TRUE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_TRUE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));

    // Delete the sphere: the selection should be empty.
    // 
    // Attempting to delete with MDGModifier crashes with an assert:
    // 
    // ASSERTION: TdependGraph::getInstance().containsNode(object)
    // File: Z:\worktrees\master\Maya\src\OGSMayaBridge\ObjectManagement\OGSDagItem.cpp Line: 1696    
    //
    // MObject sphereObj;
    // ASSERT_EQ(sphereSn.getDependNode(0, sphereObj), MS::kSuccess);
    // MDGModifier dgMod;
    // ASSERT_EQ(dgMod.deleteNode(sphereObj), MS::kSuccess);
    // ASSERT_EQ(dgMod.doIt(), MS::kSuccess);
    // 
    // Use the MEL delete command instead.  PPT, 19-Oct-2023.
    //
    ASSERT_EQ(MGlobal::executeCommand("delete |aSphere"), MS::kSuccess);
    ASSERT_EQ(MGlobal::executeCommand("refresh"), MS::kSuccess);

    hdSn = ssio.GetSelection();
    ASSERT_TRUE(hdSn->GetAllSelectedPrimPaths().empty());
    ASSERT_FALSE(selectionSi->IsFullySelected(sceneIndexPath));
    ASSERT_FALSE(selectionSi->HasFullySelectedAncestorInclusive(sceneIndexPath));
}
