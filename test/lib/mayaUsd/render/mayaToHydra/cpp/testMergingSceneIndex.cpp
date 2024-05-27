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
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <flowViewport/sceneIndex/fvpMergingSceneIndex.h>

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

TEST(FlowViewport, mergingSceneIndex)
{
    // The Flow Viewport custom merging scene index is in the scene index tree.
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto isFvpMergingSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Merging Scene Index");
    auto mergingSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpMergingSceneIndex);
    ASSERT_TRUE(mergingSiBase);

    // The custom merging scene index has the MayaHydraSceneIndex as a child,
    // to produce Maya Dag data.
    auto mergingSi = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(mergingSiBase);
    ASSERT_TRUE(mergingSi);
    
    auto producers = mergingSi->GetInputScenes();
    auto isMayaProducerSceneIndex = SceneIndexDisplayNamePred(
        "MayaHydraSceneIndex");
    auto found = std::find_if(
        producers.begin(), producers.end(), isMayaProducerSceneIndex);

    ASSERT_NE(found, producers.end());
    auto mayaSi = TfDynamic_cast<PXR_NS::MayaHydraSceneIndexRefPtr>(*found);
    ASSERT_TRUE(mayaSi);

    // The Flow Viewport merging scene index supports the Flow Viewport path
    // interface, and forwards the call to the Maya scene index, which will
    // translate a Maya path into a scene index SdfPath.

    // Get the path to the sphere defined in the Python driver for this test.
    // We know this is a single-segment UFE path, and that its tail is
    // "aSphere".
    auto mayaPath = Ufe::PathString::path("|aSphere");
    ASSERT_EQ(mayaPath.nbSegments(), 1u);
    ASSERT_EQ(mayaPath.back().string(), "aSphere");

    // The Maya data producer scene index supports the path interface.  Ask it
    // to translate the application path into a scene index path.
    auto primSelections = mayaSi->ConvertUfeSelectionToHydra(mayaPath);
    ASSERT_EQ(primSelections.size(), 1u);

    // Regardless of prefix, the scene index path tail component will match the
    // Maya node name
    ASSERT_EQ(primSelections.front().primPath.GetName(), mayaPath.back().string());

    // If we ask the terminal scene index for a prim at that path, there must be
    // one.  Prims that exist have a non-null data source.
    SdfPath nonExistentPrimPath("/foo/bar");
    auto nonExistentPrim = sceneIndices.front()->GetPrim(nonExistentPrimPath);
    ASSERT_FALSE(nonExistentPrim.dataSource);
    
    auto spherePrim = sceneIndices.front()->GetPrim(primSelections.front().primPath);
    ASSERT_TRUE(spherePrim.dataSource);

    // Flow Viewport merging scene index must give the same scene index path
    // answer as the Maya data producer scene index.
    auto mergingSiSelections = mergingSi->ConvertUfeSelectionToHydra(mayaPath);
    ASSERT_EQ(mergingSiSelections.size(), 1u);
    ASSERT_EQ(mergingSiSelections.front().primPath, primSelections.front().primPath);
}

TEST(FlowViewport, mergingSceneIndexAddRemove)
{
    // Same setup as mergingSceneIndex test.
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto isFvpMergingSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Merging Scene Index");
    auto mergingSiBase = findSceneIndexInTree(
        sceneIndices.front(), isFvpMergingSceneIndex);
    auto mergingSi = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(mergingSiBase);
    auto producers = mergingSi->GetInputScenes();
    auto isMayaProducerSceneIndex = SceneIndexDisplayNamePred(
        "MayaHydraSceneIndex");
    auto found = std::find_if(
        producers.begin(), producers.end(), isMayaProducerSceneIndex);
    auto mayaSi = TfDynamic_cast<PXR_NS::MayaHydraSceneIndexRefPtr>(*found);
    auto mayaPath = Ufe::PathString::path("|aSphere");
    auto primSelections = mayaSi->ConvertUfeSelectionToHydra(mayaPath);
    ASSERT_EQ(primSelections.size(), 1u);

    // With Maya scene index in the merging scene index, the sphere prim has
    // a valid scene index path.
    auto spherePrim = sceneIndices.front()->GetPrim(primSelections.front().primPath);
    ASSERT_TRUE(spherePrim.dataSource);
    spherePrim = mergingSi->GetPrim(primSelections.front().primPath);
    ASSERT_TRUE(spherePrim.dataSource);

    // Remove the Maya scene index from the Flow Viewport merging scene index.
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 1u);
    mergingSi->RemoveInputScene(mayaSi);
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 0u);

    // Without the Maya scene index in the merging scene index, the sphere prim
    // is no longer in the Hydra scene index scene.
    spherePrim = sceneIndices.front()->GetPrim(primSelections.front().primPath);
    ASSERT_FALSE(spherePrim.dataSource);
    spherePrim = mergingSi->GetPrim(primSelections.front().primPath);
    ASSERT_FALSE(spherePrim.dataSource);

    // Add the Maya scene index back to the Flow Viewport merging scene index,
    // sphere prim will reappear.  We know that the Maya scene index is added
    // with the absolute root path as scene root, so duplicate that here.
    mergingSi->AddInputScene(mayaSi, SdfPath::AbsoluteRootPath());
    ASSERT_EQ(mergingSi->GetInputScenes().size(), 1u);
    spherePrim = sceneIndices.front()->GetPrim(primSelections.front().primPath);
    ASSERT_TRUE(spherePrim.dataSource);
    spherePrim = mergingSi->GetPrim(primSelections.front().primPath);
    ASSERT_TRUE(spherePrim.dataSource);
}
