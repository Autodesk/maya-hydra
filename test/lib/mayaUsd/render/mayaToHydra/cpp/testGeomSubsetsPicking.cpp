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
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/utils.h>
#include <pxr/usd/sdf/path.h>

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

const std::string kCubeMesh = "CubeMesh";
const std::string kCubeUpperHalf = "CubeUpperHalf";
const std::string kSphereMesh = "SphereMesh";
const std::string kSphereUpperHalf = "SphereUpperHalf";

const std::string kStageUfePathSegment = "|GeomSubsetsPickingTestScene|GeomSubsetsPickingTestSceneShape";
const std::string kCubeMeshUfePathSegment = "/Root/CubeMeshXform/CubeMesh";
const std::string kSphereMeshUfePathSegment = "/Root/SphereMeshXform/SphereMesh";
const std::string kSphereInstancerUfePathSegment = "/Root/SphereInstancer";

FindPrimPredicate findPickPrimPredicate(const std::string& objectName, const TfToken& primType)
{
    return [objectName,
            primType](const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) -> bool {
        return primPath.GetAsString().find(objectName) != std::string::npos
            && sceneIndex->GetPrim(primPath).primType == primType;
    };
}

void ensureSelected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);

    size_t nbSelectedPrims = 0;
    for (const auto& primEntry : primEntries) {
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        if (selectionsSchema.GetNumElements() > 0u) {
            ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
            HdSelectionSchema selectionSchema = selectionsSchema.GetElement(0);
            EXPECT_TRUE(selectionSchema.GetFullySelected());
            nbSelectedPrims++;
        }
    }

    ASSERT_GT(nbSelectedPrims, 0u);
}

void ensureUnselected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);
    for (const auto& primEntry : primEntries) {
        HdSelectionsSchema selectionsSchema
            = HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        ASSERT_EQ(selectionsSchema.IsDefined(), false);
    }
}

void testPrimPicking(const Ufe::Path& clickObjectUfePath, const QPoint& clickOffset, const Ufe::Path& selectedObjectUfePath)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto ufeSelection = Ufe::GlobalSelection::get();
    ASSERT_TRUE(ufeSelection->empty());

    const auto selectionSceneIndex = findSelectionSceneIndexInTree(inspector.GetSceneIndex());
    ASSERT_TRUE(selectionSceneIndex);

    const auto selectedObjectSceneIndexPath = selectionSceneIndex->SceneIndexPath(selectedObjectUfePath);

    HdSceneIndexPrim selectedObjectSceneIndexPrim = inspector.GetSceneIndex()->GetPrim(selectedObjectSceneIndexPath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(selectedObjectSceneIndexPrim.dataSource);
    ASSERT_EQ(selectionsSchema.IsDefined(), false);

    M3dView active3dView = M3dView::active3dView();
    const auto clickObjectSceneIndexPath = selectionSceneIndex->SceneIndexPath(clickObjectUfePath);
    auto primMouseCoords = getPrimMouseCoords(inspector.GetSceneIndex()->GetPrim(clickObjectSceneIndexPath), active3dView);
    primMouseCoords += clickOffset;

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

    ASSERT_EQ(ufeSelection->size(), 1u);
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

    M3dView active3dView = M3dView::active3dView();
    const auto clickInstancerSceneIndexPath = selectionSceneIndex->SceneIndexPath(clickInstancerUfePath);
    auto primMouseCoords = getInstanceMouseCoords(inspector.GetSceneIndex()->GetPrim(clickInstancerSceneIndexPath), clickInstanceIndex, active3dView);
    primMouseCoords += clickOffset;

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    active3dView.refresh();

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

} // namespace

TEST(TestGeomSubsetsPicking, geomSubsetPicking)
{
    const auto cubeMeshUfePathString = kStageUfePathSegment + "," + kCubeMeshUfePathSegment;
    const auto cubeMeshUfePath = Ufe::PathString::path(cubeMeshUfePathString);
    const auto cubeUpperHalfUfePath = Ufe::PathString::path(cubeMeshUfePathString + "/" + kCubeUpperHalf);
    testPrimPicking(cubeMeshUfePath, QPoint(0, -25), cubeUpperHalfUfePath);
}

TEST(TestGeomSubsetsPicking, fallbackPicking)
{
    const auto cubeMeshUfePathString = kStageUfePathSegment + "," + kCubeMeshUfePathSegment;
    const auto cubeMeshUfePath = Ufe::PathString::path(cubeMeshUfePathString);
    testPrimPicking(cubeMeshUfePath, QPoint(0, 25), cubeMeshUfePath);
}

TEST(TestGeomSubsetsPicking, instanceGeomSubsetPicking)
{
    const auto sphereInstancerUfePathString = kStageUfePathSegment + "," + kSphereInstancerUfePathSegment;
    const auto sphereInstancerUfePath = Ufe::PathString::path(sphereInstancerUfePathString);

    const auto sphereMeshUfePathString = kStageUfePathSegment + "," + kSphereMeshUfePathSegment;
    const auto sphereUpperHalfUfePath = Ufe::PathString::path(sphereMeshUfePathString + "/" + kSphereUpperHalf);

    testInstancePicking(sphereInstancerUfePath, 0, QPoint(0, -25), sphereUpperHalfUfePath);
}

TEST(TestGeomSubsetsPicking, instanceFallbackPicking)
{
    const auto sphereInstancerUfePathString = kStageUfePathSegment + "," + kSphereInstancerUfePathSegment;
    const auto sphereInstancerUfePath = Ufe::PathString::path(sphereInstancerUfePathString);

    const auto sphereMeshUfePathString = kStageUfePathSegment + "," + kSphereMeshUfePathSegment;
    const auto sphereMeshUfePath = Ufe::PathString::path(sphereMeshUfePathString);

    testInstancePicking(sphereInstancerUfePath, 0, QPoint(0, 25), sphereMeshUfePath);
}

TEST(TestGeomSubsetsPicking, marqueeSelect)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    std::vector<std::string> geomSubsetNamesToSelect = {"CubeUpperHalf", "SphereUpperHalf"};
    
    for (const auto& geomSubsetName : geomSubsetNamesToSelect) {
        ensureUnselected(inspector, PrimNamePredicate(geomSubsetName));
    }

    auto ufeSelection = Ufe::GlobalSelection::get();
    EXPECT_TRUE(ufeSelection->empty());

    M3dView active3dView = M3dView::active3dView();

    // Initialize the selection rectangle
    int offset = 10;
    QPoint topLeftMouseCoords(0 + offset, 0 + offset);
    QPoint bottomRightMouseCoords(active3dView.portWidth() - offset, active3dView.portHeight() - offset);

    // Perform the marquee selection
    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), topLeftMouseCoords);
    mouseMoveTo(active3dView.widget(), bottomRightMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), bottomRightMouseCoords);

    active3dView.refresh(false, true);

    std::ofstream outFile("HydraSceneDump.txt");
    HdUtils::PrintSceneIndex(outFile, inspector.GetSceneIndex());
    outFile.close();

    for (const auto& geomSubsetName : geomSubsetNamesToSelect) {
        ensureSelected(inspector, PrimNamePredicate(geomSubsetName));
    }
}
