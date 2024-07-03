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
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/utils.h>

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

} // namespace

TEST(TestGeomSubsetsPicking, geomSubsetPicking)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const std::string objectName = "CubeMesh";
    const std::string geomSubsetName = "CubeUpperHalf";

    ensureUnselected(inspector, PrimNamePredicate(geomSubsetName));

    PrimEntriesVector prims = inspector.FindPrims(findPickPrimPredicate(objectName, HdPrimTypeTokens->mesh));
    ASSERT_EQ(prims.size(), 1u);

    M3dView active3dView = M3dView::active3dView();

    auto primMouseCoords = getPrimMouseCoords(prims.front().prim, active3dView);
    primMouseCoords -= QPoint(0, 25); // Move coords upwards

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh(false, true);

    ensureSelected(inspector, PrimNamePredicate(geomSubsetName));
}

TEST(TestGeomSubsetsPicking, fallbackPicking)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const std::string objectName = "CubeMesh";

    ensureUnselected(inspector, PrimNamePredicate(objectName));

    PrimEntriesVector prims = inspector.FindPrims(findPickPrimPredicate(objectName, HdPrimTypeTokens->mesh));
    ASSERT_EQ(prims.size(), 1u);

    M3dView active3dView = M3dView::active3dView();

    auto primMouseCoords = getPrimMouseCoords(prims.front().prim, active3dView);
    primMouseCoords += QPoint(0, 25); // Move coords downwards

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh(false, true);

    ensureSelected(inspector, PrimNamePredicate(objectName));
}

TEST(TestGeomSubsetsPicking, instanceGeomSubsetPicking)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const std::string objectName = "SphereMesh";
    const std::string geomSubsetName = "SphereUpperHalf";
    const std::string instancerName = "SphereInstancer";

    ensureUnselected(inspector, PrimNamePredicate(geomSubsetName));

    PrimEntriesVector instancerPrims = inspector.FindPrims(findPickPrimPredicate(instancerName, HdPrimTypeTokens->instancer));
    ASSERT_EQ(instancerPrims.size(), 1u);


    M3dView active3dView = M3dView::active3dView();

    auto primMouseCoords = getInstanceMouseCoords(instancerPrims.front().prim, 0, active3dView);
    primMouseCoords -= QPoint(0, 25); // Move coords upwards

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh(false, true);

    ensureSelected(inspector, PrimNamePredicate(geomSubsetName));
}

TEST(TestGeomSubsetsPicking, instanceFallbackPicking)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const std::string objectName = "SphereMesh";
    const std::string geomSubsetName = "SphereUpperHalf";
    const std::string instancerName = "SphereInstancer";

    ensureUnselected(inspector, PrimNamePredicate(objectName));

    PrimEntriesVector instancerPrims = inspector.FindPrims(findPickPrimPredicate(instancerName, HdPrimTypeTokens->instancer));
    ASSERT_EQ(instancerPrims.size(), 1u);


    M3dView active3dView = M3dView::active3dView();

    auto primMouseCoords = getInstanceMouseCoords(instancerPrims.front().prim, 0, active3dView);
    primMouseCoords += QPoint(0, 25); // Move coords downwards

    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh(false, true);

    ensureSelected(inspector, PrimNamePredicate(objectName));
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
