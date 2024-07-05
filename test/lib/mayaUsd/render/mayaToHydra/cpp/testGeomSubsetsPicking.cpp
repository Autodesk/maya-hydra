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
const std::string kSphereInstancerUfePathSegment = "/Root/SphereInstancer";

const std::string kCubeUpperHalfName = "CubeUpperHalf";
const std::string kSphereUpperHalfName = "SphereUpperHalf";

void debugPrintUfePath(const std::string varName, const Ufe::Path& path) {
    std::cout << "Printing " << varName << std::endl;
    std::cout << "\t" << "Path : " << path.string() << std::endl;
    for (const auto& seg : path.getSegments()) {
        std::cout << "\t\t" << "Segment Rtid : " << seg.runTimeId() << std::endl;
        std::cout << "\t\t" << "Segment separator : " << seg.separator() << std::endl;
        for (const auto& comp : seg.components()) {
            std::cout << "\t\t\t" << "Component : " << comp.string() << std::endl;
        }
    }
}

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

void testPrimPicking(const Ufe::Path& clickObjectUfePath, const QPoint& clickOffset, const Ufe::Path& selectedObjectUfePath)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Preconditions
    auto ufeSelection = Ufe::GlobalSelection::get();
    ASSERT_TRUE(ufeSelection->empty());

    const auto selectionSceneIndex = findSelectionSceneIndexInTree(inspector.GetSceneIndex());
    ASSERT_TRUE(selectionSceneIndex);

    debugPrintUfePath("selectedObjectUfePath", selectedObjectUfePath);
    const auto selectedObjectSceneIndexPath = selectionSceneIndex->SceneIndexPath(selectedObjectUfePath);
    std::cout << "selectedObjectSceneIndexPath : " << selectedObjectSceneIndexPath.GetString() << std::endl;

    HdSceneIndexPrim selectedObjectSceneIndexPrim = inspector.GetSceneIndex()->GetPrim(selectedObjectSceneIndexPath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(selectedObjectSceneIndexPrim.dataSource);
    ASSERT_EQ(selectionsSchema.IsDefined(), false);

    // Picking
    M3dView active3dView = M3dView::active3dView();
    debugPrintUfePath("clickObjectUfePath", clickObjectUfePath);
    const auto clickObjectSceneIndexPath = selectionSceneIndex->SceneIndexPath(clickObjectUfePath);
    std::cout << "clickObjectSceneIndexPath : " << clickObjectSceneIndexPath.GetString() << std::endl;
    auto primMouseCoords = getPrimMouseCoords(inspector.GetSceneIndex()->GetPrim(clickObjectSceneIndexPath), active3dView);
    primMouseCoords += clickOffset;

    std::cout << "primMouseCoords : " << primMouseCoords.x() << ", " << primMouseCoords.y() << std::endl;
    std::cout << "viewportSize : " << active3dView.portWidth() << ", " << active3dView.portHeight() << std::endl;
    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

    // Postconditions
    ASSERT_EQ(ufeSelection->size(), 1u);
    debugPrintUfePath("ufeSelection->front()->path()", ufeSelection->front()->path());
    ASSERT_TRUE(ufeSelection->contains(selectedObjectUfePath));

    selectedObjectSceneIndexPrim = inspector.GetSceneIndex()->GetPrim(selectedObjectSceneIndexPath);
    selectionsSchema = HdSelectionsSchema::GetFromParent(selectedObjectSceneIndexPrim.dataSource);
    ASSERT_EQ(selectionsSchema.IsDefined(), true);
    ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
    ASSERT_TRUE(selectionsSchema.GetElement(0).GetFullySelected());
}

void testInstancePicking(const Ufe::Path& clickInstancerUfePath, int clickInstanceIndex, const QPoint& clickOffset, const Ufe::Path& selectedObjectUfePath)
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
    const auto clickInstancerSceneIndexPath = selectionSceneIndex->SceneIndexPath(clickInstancerUfePath);
    auto primMouseCoords = getInstanceMouseCoords(inspector.GetSceneIndex()->GetPrim(clickInstancerSceneIndexPath), clickInstanceIndex, active3dView);
    primMouseCoords += clickOffset;

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
    const auto cubeMeshUfePathString = kStageUfePathSegment + "," + kCubeMeshUfePathSegment;
    const auto cubeMeshUfePath = Ufe::PathString::path(cubeMeshUfePathString);
    const auto cubeUpperHalfUfePath = Ufe::PathString::path(cubeMeshUfePathString + "/" + kCubeUpperHalfName);
    testPrimPicking(cubeMeshUfePath, QPoint(0, -25), cubeUpperHalfUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, fallbackPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto cubeMeshUfePathString = kStageUfePathSegment + "," + kCubeMeshUfePathSegment;
    const auto cubeMeshUfePath = Ufe::PathString::path(cubeMeshUfePathString);
    testPrimPicking(cubeMeshUfePath, QPoint(0, 25), cubeMeshUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, instanceGeomSubsetPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto sphereInstancerUfePathString = kStageUfePathSegment + "," + kSphereInstancerUfePathSegment;
    const auto sphereInstancerUfePath = Ufe::PathString::path(sphereInstancerUfePathString);

    const auto sphereMeshUfePathString = kStageUfePathSegment + "," + kSphereMeshUfePathSegment;
    const auto sphereUpperHalfUfePath = Ufe::PathString::path(sphereMeshUfePathString + "/" + kSphereUpperHalfName);

    testInstancePicking(sphereInstancerUfePath, 0, QPoint(0, -25), sphereUpperHalfUfePath);
#endif
}

TEST(TestGeomSubsetsPicking, instanceFallbackPicking)
{
#if PXR_VERSION < 2403
    GTEST_SKIP() << "Skipping test, USD version used does not support GeomSubset prims.";
#else
    const auto sphereInstancerUfePathString = kStageUfePathSegment + "," + kSphereInstancerUfePathSegment;
    const auto sphereInstancerUfePath = Ufe::PathString::path(sphereInstancerUfePathString);

    const auto sphereMeshUfePathString = kStageUfePathSegment + "," + kSphereMeshUfePathSegment;
    const auto sphereMeshUfePath = Ufe::PathString::path(sphereMeshUfePathString);

    testInstancePicking(sphereInstancerUfePath, 0, QPoint(0, 25), sphereMeshUfePath);
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
        assertUnselected(inspector, PrimNamePredicate(geomSubsetName));
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
        assertSelected(inspector, PrimNamePredicate(geomSubsetName));
    }
#endif
}
