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

#include <mayaHydraLib/mayaHydra.h>

#include <flowViewport/sceneIndex/fvpMergingSceneIndex.h>
#include <flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.h>

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hdx/selectionSceneIndexObserver.h>

#include <ufe/globalSelection.h>
#include <ufe/hierarchy.h>
#include <ufe/observableSelection.h>
#include <ufe/pathSegment.h>
#include <ufe/pathString.h>
#include <ufe/selection.h>
#include <ufe/sceneItem.h>

#include <gtest/gtest.h>

#include <algorithm>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

const std::string kStageUfePathSegment = "|GeomSubsetsPickingTestScene|GeomSubsetsPickingTestSceneShape";

const std::string kCubeMeshUfePathSegment = "/Root/CubeMeshXform/CubeMesh";
const std::string kSphereMeshUfePathSegment = "/Root/SphereMeshXform/SphereMesh";

const std::string kCubeUpperHalfName = "CubeUpperHalf";
const std::string kSphereUpperHalfName = "SphereUpperHalf";

const std::string kCubeUpperHalfMarkerUfePathSegment = "/Root/CubeUpperHalfMarker";
const std::string kCubeLowerHalfMarkerUfePathSegment = "/Root/CubeLowerHalfMarker";
const std::string kSphereInstanceUpperHalfMarkerUfePathSegment = "/Root/SphereInstanceUpperHalfMarker";
const std::string kSphereInstanceLowerHalfMarkerUfePathSegment = "/Root/SphereInstanceLowerHalfMarker";

SdfPath getSelectionHighlightMirrorPathFromOriginal(const SdfPath& originalPath, const std::string& selectionHighlightMirrorTag)
{
    return originalPath.ReplaceName(TfToken(originalPath.GetName() + selectionHighlightMirrorTag));
}

bool isSelectionHighlightMirror(const SdfPath& primPath, const std::string& selectionHighlightMirrorTag)
{
    return primPath.GetElementString().find(selectionHighlightMirrorTag) != std::string::npos;
}

bool findMeshPrimsPredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    return sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
}

} // namespace

TEST(GeomSubsetsWireframeHighlight, instancedGeomSubsetHighlight)
{
    const SceneIndicesVector& terminalSceneIndices = GetTerminalSceneIndices();
    ASSERT_FALSE(terminalSceneIndices.empty());
    SceneIndexInspector inspector(terminalSceneIndices.front());

    auto isFvpMergingSceneIndexPredicate = SceneIndexDisplayNamePred("Flow Viewport Merging Scene Index");
    auto fvpMergingSceneIndex = TfDynamic_cast<Fvp::MergingSceneIndexRefPtr>(
        findSceneIndexInTree(terminalSceneIndices.front(), isFvpMergingSceneIndexPredicate));

    auto isFvpWireframeSelectionHighlightSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Wireframe Selection Highlight Scene Index");
    auto fvpWireframeSelectionHighlightSceneIndex = TfDynamic_cast<Fvp::WireframeSelectionHighlightSceneIndexRefPtr>(
        findSceneIndexInTree(terminalSceneIndices.front(), isFvpWireframeSelectionHighlightSceneIndex));
    std::string selectionHighlightMirrorTag = fvpWireframeSelectionHighlightSceneIndex->GetSelectionHighlightMirrorTag();

    auto ufeSelection = Ufe::GlobalSelection::get();

    HdxSelectionSceneIndexObserver selectionObserver;
    selectionObserver.SetSceneIndex(terminalSceneIndices.front());

    // Create this test's selected scene items
    auto geomSubsetPath = Ufe::PathString::path(kStageUfePathSegment + "," + kSphereMeshUfePathSegment + "/" + kSphereUpperHalfName);
    auto geomSubsetItem = Ufe::Hierarchy::createItem(geomSubsetPath);

    // Initial state : ensure nothing is highlighted
    ufeSelection->clear();

    auto selectionHighlightMirrors = inspector.FindPrims([selectionHighlightMirrorTag](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        return isSelectionHighlightMirror(primPath, selectionHighlightMirrorTag);
    });
    EXPECT_TRUE(selectionHighlightMirrors.empty()); // No selection highlight mirrors

    // Select prototype
    ufeSelection->replaceWith(geomSubsetItem);

    auto geomSubsetPrimSelections = fvpMergingSceneIndex->UfePathToPrimSelections(geomSubsetPath);
    // Original prim + propagated prototype
    EXPECT_EQ(geomSubsetPrimSelections.size(), 1u + 1u);

    // Validate scene structure
    EXPECT_FALSE(inspector.FindPrims(findMeshPrimsPredicate).empty());
    for (size_t iSelection = 0; iSelection < geomSubsetPrimSelections.size(); iSelection++) {
        const auto& geomSubsetPrimSelection = geomSubsetPrimSelections[iSelection];
        auto selectionHighlightPath = fvpWireframeSelectionHighlightSceneIndex->GetSelectionHighlightPath(geomSubsetPrimSelection.primPath);
        ASSERT_NE(selectionHighlightPath, geomSubsetPrimSelection.primPath);
        assertSelectionHighlightCorrectness(inspector.GetSceneIndex(), selectionHighlightPath, selectionHighlightMirrorTag);
        HdSceneIndexPrim geomSubsetPrim = inspector.GetSceneIndex()->GetPrim(geomSubsetPrimSelection.primPath);
        dataSourceMatchesReference(
            HdContainerDataSource::Get(geomSubsetPrim.dataSource, HdMeshSchema::GetTopologyLocator()),
            getPathToSample("geomSubset_topology" + std::to_string(iSelection) + ".txt"));
    }

    // Ensure the accumulated selected paths correspond to the intended/translated paths
    auto selectedPrimPaths = selectionObserver.GetSelection()->GetAllSelectedPrimPaths();
    ASSERT_EQ(selectedPrimPaths.size(), geomSubsetPrimSelections.size());
    for (const auto& geomSubsetPrimSelection : geomSubsetPrimSelections) {
        auto foundSelectedPrimPath = std::find(selectedPrimPaths.begin(), selectedPrimPaths.end(), geomSubsetPrimSelection.primPath);
        EXPECT_NE(foundSelectedPrimPath, selectedPrimPaths.end());
    }
}
