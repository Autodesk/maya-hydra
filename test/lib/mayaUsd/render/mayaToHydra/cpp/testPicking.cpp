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
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/M3dView.h>
#include <maya/MPoint.h>

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

void getPrimMouseCoords(const HdSceneIndexPrim& prim, M3dView& view, QPoint& outMouseCoords)
{
    HdDataSourceBaseHandle xformDataSource = HdContainerDataSource::Get(prim.dataSource, HdXformSchema::GetDefaultLocator());
    ASSERT_NE(xformDataSource, nullptr);
    HdContainerDataSourceHandle xformContainerDataSource = HdContainerDataSource::Cast(xformDataSource);
    ASSERT_NE(xformContainerDataSource, nullptr);
    HdXformSchema xformSchema(xformContainerDataSource);
    ASSERT_NE(xformSchema.GetMatrix(), nullptr);
    GfMatrix4d xformMatrix = xformSchema.GetMatrix()->GetTypedValue(0);
    GfVec3d translation = xformMatrix.ExtractTranslation();

    MPoint worldPosition(translation[0], translation[1], translation[2], 1.0);
    short   viewportX = 0, viewportY = 0;
    MStatus worldToViewStatus;
    // First assert checks that the point was not clipped, second assert checks the general MStatus
    ASSERT_TRUE(view.worldToView(worldPosition, viewportX, viewportY, &worldToViewStatus));
    ASSERT_TRUE(worldToViewStatus);

    // Qt and M3dView use opposite Y-coordinates
    outMouseCoords = QPoint(viewportX, view.portHeight() - viewportY);
}

void ensureSelected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    // 2024-03-01 : Due to the extra "Lighted" hierarchy, it is possible for an object to be split
    // into two prims, only one of which will be selected. We will tolerate this in the test, but 
    // we'll make sure there are at most two prims for that object. We'll also allow a prim not 
    // to have any selections, but at least one prim must be selected.
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);
    ASSERT_GE(primEntries.size(), 1u);
    ASSERT_LE(primEntries.size(), 2u);

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

TEST(TestPicking, pickObject)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 2);
    const std::string objectName(argv[0]);
    const TfToken primType(argv[1]);

    ensureUnselected(inspector, PrimNamePredicate(objectName));

    PrimEntriesVector prims = inspector.FindPrims(findPickPrimPredicate(objectName, primType));
    ASSERT_EQ(prims.size(), 1u);

    M3dView active3dView = M3dView::active3dView();

    QPoint primMouseCoords;
    getPrimMouseCoords(prims.front().prim, active3dView, primMouseCoords);

    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh();

    ensureSelected(inspector, PrimNamePredicate(objectName));
}

TEST(TestPicking, marqueeSelect)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto [argc, argv] = getTestingArgs();
    ASSERT_TRUE(argc % 2 == 0); // Each object is identified by both its name and a type
    ASSERT_TRUE(argc >= 4); // We need at least two objects to do the marquee selection
    std::vector<std::pair<std::string, TfToken>> objectsToSelect;
    for (int iArg = 0; iArg < argc; iArg += 2) {
        objectsToSelect.push_back(std::make_pair(std::string(argv[iArg]), TfToken(argv[iArg + 1])));
    }

    for (const auto& object : objectsToSelect) {
        ensureUnselected(inspector, PrimNamePredicate(object.first));
    }

    M3dView active3dView = M3dView::active3dView();

    // We get the first prim's mouse coordinates and initialize the selection rectangle
    // with them; we then iterate on the other prims and expand the selection rectangle
    // to fit them all.

    // Get the first prim's mouse coords
    PrimEntriesVector initialPrimEntries = inspector.FindPrims(
        findPickPrimPredicate(objectsToSelect.front().first, objectsToSelect.front().second));
    ASSERT_EQ(initialPrimEntries.size(), 1u);
    QPoint initialMouseCoords;
    getPrimMouseCoords(initialPrimEntries.front().prim, active3dView, initialMouseCoords);

    // Initialize the selection rectangle
    QPoint topLeftMouseCoords = initialMouseCoords;
    QPoint bottomRightMouseCoords = initialMouseCoords;

    // Expand the selection rectangle to fit all prims
    for (size_t iObject = 1; iObject < objectsToSelect.size(); iObject++) {
        PrimEntriesVector objectPrims = inspector.FindPrims(
            findPickPrimPredicate(objectsToSelect[iObject].first, objectsToSelect[iObject].second));
        ASSERT_EQ(objectPrims.size(), 1u);
        QPoint objectMouseCoords;
        getPrimMouseCoords(objectPrims.front().prim, active3dView, objectMouseCoords);

        if (objectMouseCoords.x() > bottomRightMouseCoords.x()) {
            bottomRightMouseCoords.setX(objectMouseCoords.x());
        }
        if (objectMouseCoords.x() < topLeftMouseCoords.x()) {
            topLeftMouseCoords.setX(objectMouseCoords.x());
        }
        if (objectMouseCoords.y() > topLeftMouseCoords.y()) {
            topLeftMouseCoords.setY(objectMouseCoords.y());
        }
        if (objectMouseCoords.y() < bottomRightMouseCoords.y()) {
            bottomRightMouseCoords.setY(objectMouseCoords.y());
        }
    }

    // Perform the marquee selection
    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), topLeftMouseCoords);
    mouseMoveTo(active3dView.widget(), bottomRightMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), bottomRightMouseCoords);

    active3dView.refresh();

    for (const auto& object : objectsToSelect) {
        ensureSelected(inspector, PrimNamePredicate(object.first));
    }
}
