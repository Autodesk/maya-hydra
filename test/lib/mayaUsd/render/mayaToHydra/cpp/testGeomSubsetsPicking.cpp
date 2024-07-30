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

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/path.h>
#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

#if PXR_VERSION >= 2403
const std::string kStageUfePathSegment = "|GeomSubsetsPickingTestScene|GeomSubsetsPickingTestSceneShape";

const std::string kCubeMeshUfePathSegment = "/Root/CubeMeshXform/CubeMesh";
const std::string kSphereMeshUfePathSegment = "/Root/SphereMeshXform/SphereMesh";

const std::string kCubeUpperHalfName = "CubeUpperHalf";
const std::string kSphereUpperHalfName = "SphereUpperHalf";

const std::string kCubeUpperHalfMarkerUfePathSegment = "/Root/CubeUpperHalfMarker";
const std::string kCubeLowerHalfMarkerUfePathSegment = "/Root/CubeLowerHalfMarker";
const std::string kSphereInstanceUpperHalfMarkerUfePathSegment = "/Root/SphereInstanceUpperHalfMarker";
const std::string kSphereInstanceLowerHalfMarkerUfePathSegment = "/Root/SphereInstanceLowerHalfMarker";

class FindGeomSubsetPredicate
{
public:
    FindGeomSubsetPredicate(const std::string& geomSubsetName) : _geomSubsetName(geomSubsetName) {}

    bool operator()(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return primPath.GetName() == _geomSubsetName && prim.primType == HdPrimTypeTokens->geomSubset && prim.dataSource;
    }

private:
    const std::string _geomSubsetName;
};

void assertUnselected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);
    for (const auto& primEntry : primEntries) {
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        ASSERT_FALSE(selectionsSchema.IsDefined());
    }
}

void assertSelected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);
    ASSERT_FALSE(primEntries.empty());

    for (const auto& primEntry : primEntries) {
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        ASSERT_TRUE(selectionsSchema.IsDefined());
        ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
        HdSelectionSchema selectionSchema = selectionsSchema.GetElement(0);
        ASSERT_TRUE(selectionSchema.GetFullySelected());
    }
}

void testPicking(const Ufe::Path& clickMarkerUfePath, const Ufe::Path& selectedObjectUfePath)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Preconditions
    auto ufeSelection = Ufe::GlobalSelection::get();
    ASSERT_TRUE(ufeSelection->empty());

    const auto selectionSceneIndex = findSelectionSceneIndexInTree(inspector.GetSceneIndex());
    ASSERT_TRUE(selectionSceneIndex);

    const auto selectedObjectSceneIndexPaths = selectionSceneIndex->SceneIndexPaths(selectedObjectUfePath);

    for (const auto& selectedObjectSceneIndexPath : selectedObjectSceneIndexPaths) {
        HdSceneIndexPrim selectedObjectSceneIndexPrim = inspector.GetSceneIndex()->GetPrim(selectedObjectSceneIndexPath);
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(selectedObjectSceneIndexPrim.dataSource);
        ASSERT_EQ(selectionsSchema.IsDefined(), false);
    }

    // Picking
    M3dView active3dView = M3dView::active3dView();
    const auto clickMarkerSceneIndexPath = selectionSceneIndex->SceneIndexPath(clickMarkerUfePath);
    auto primMouseCoords = getPrimMouseCoords(inspector.GetSceneIndex()->GetPrim(clickMarkerSceneIndexPath), active3dView);

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

    // Postconditions
    ASSERT_EQ(ufeSelection->size(), 1u);
    ASSERT_TRUE(ufeSelection->contains(selectedObjectUfePath));

    for (const auto& selectedObjectSceneIndexPath : selectedObjectSceneIndexPaths) {
        HdSceneIndexPrim selectedObjectSceneIndexPrim = inspector.GetSceneIndex()->GetPrim(selectedObjectSceneIndexPath);
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(selectedObjectSceneIndexPrim.dataSource);
        ASSERT_EQ(selectionsSchema.IsDefined(), true);
        ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
        ASSERT_TRUE(selectionsSchema.GetElement(0).GetFullySelected());
    }
}
#endif

} // namespace

TEST(TestGeomSubsetsPicking, geomSubsetPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto cubeUpperHalfMarkerUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kCubeUpperHalfMarkerUfePathSegment);
    const auto cubeUpperHalfUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kCubeMeshUfePathSegment + "/" + kCubeUpperHalfName);
    testPicking(cubeUpperHalfMarkerUfePath, cubeUpperHalfUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, fallbackPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto cubeLowerHalfMarkerUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kCubeLowerHalfMarkerUfePathSegment);
    const auto cubeMeshUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kCubeMeshUfePathSegment);
    testPicking(cubeLowerHalfMarkerUfePath, cubeMeshUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, instanceGeomSubsetPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto sphereInstanceUpperHalfMarkerUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kSphereInstanceUpperHalfMarkerUfePathSegment);
    const auto sphereUpperHalfUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kSphereMeshUfePathSegment + "/" + kSphereUpperHalfName);
    testPicking(sphereInstanceUpperHalfMarkerUfePath, sphereUpperHalfUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, instanceFallbackPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto sphereInstanceLowerHalfMarkerUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kSphereInstanceLowerHalfMarkerUfePathSegment);
    const auto sphereMeshUfePath = Ufe::PathString::path(kStageUfePathSegment + "," + kSphereMeshUfePathSegment);
    testPicking(sphereInstanceLowerHalfMarkerUfePath, sphereMeshUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, marqueeSelect)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const auto cubeMeshUfePathString = kStageUfePathSegment + "," + kCubeMeshUfePathSegment;
    const auto cubeUpperHalfUfePath = Ufe::PathString::path(cubeMeshUfePathString + "/" + kCubeUpperHalfName);

    const auto sphereMeshUfePathString = kStageUfePathSegment + "," + kSphereMeshUfePathSegment;
    const auto sphereUpperHalfUfePath = Ufe::PathString::path(sphereMeshUfePathString + "/" + kSphereUpperHalfName);

    std::vector<std::string> geomSubsetNamesToSelect = {"CubeUpperHalf", "SphereUpperHalf"};

    // Preconditions
    auto ufeSelection = Ufe::GlobalSelection::get();
    ASSERT_TRUE(ufeSelection->empty());
    
    for (const auto& geomSubsetName : geomSubsetNamesToSelect) {
        assertUnselected(inspector, FindGeomSubsetPredicate(geomSubsetName));
    }

    // Marquee selection
    M3dView active3dView = M3dView::active3dView();

    int offsetFromBorder = 10;
    QPoint topLeftMouseCoords(0 + offsetFromBorder, 0 + offsetFromBorder);
    QPoint bottomRightMouseCoords(active3dView.portWidth() - offsetFromBorder, active3dView.portHeight() - offsetFromBorder);

    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), topLeftMouseCoords);
    mouseMoveTo(active3dView.widget(), bottomRightMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), bottomRightMouseCoords);
    active3dView.refresh();

    // Postconditions
    ASSERT_EQ(ufeSelection->size(), 2u);
    ASSERT_TRUE(ufeSelection->contains(cubeUpperHalfUfePath));
    ASSERT_TRUE(ufeSelection->contains(sphereUpperHalfUfePath));

    for (const auto& geomSubsetName : geomSubsetNamesToSelect) {
        assertSelected(inspector, FindGeomSubsetPredicate(geomSubsetName));
    }
#endif
}
