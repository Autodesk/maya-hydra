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

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

#include <flowViewport/sceneIndex/fvpSelectionSceneIndex.h>

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>

#include <maya/M3dView.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MItSelectionList.h>

#include <QApplication>
#include <QGraphicsView>
#include <QWindow>
#include <QWidget>
#include <QOpenGLContext>
#include <QMouseEvent>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

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

Qt::MouseButtons mouseButtons;
Qt::KeyboardModifiers keyboardModifiers;

void mousePress(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos)
{
    QMouseEvent mousePressEvent(
        QEvent::Type::MouseButtonPress,
        localMousePos,
        widget->mapToGlobal(localMousePos),
        mouseButton,
        mouseButtons,
        keyboardModifiers);
    mouseButtons |= mouseButton;

    QCursor::setPos(widget->mapToGlobal(localMousePos));
    QApplication::sendEvent(widget, &mousePressEvent);
}

void mouseRelease(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos)
{
    mouseButtons &= ~mouseButton;
    QMouseEvent mouseReleaseEvent(
        QEvent::Type::MouseButtonRelease,
        localMousePos,
        widget->mapToGlobal(localMousePos),
        mouseButton,
        mouseButtons,
        keyboardModifiers);

    QCursor::setPos(widget->mapToGlobal(localMousePos));
    QApplication::sendEvent(widget, &mouseReleaseEvent);
}

void mouseMoveTo(QWidget* widget, QPoint localMousePos)
{
    QMouseEvent mouseMoveEvent(
        QEvent::Type::MouseMove,
        localMousePos,
        widget->mapToGlobal(localMousePos),
        Qt::MouseButton::NoButton,
        mouseButtons,
        keyboardModifiers);

    QCursor::setPos(widget->mapToGlobal(localMousePos));
    QApplication::sendEvent(widget, &mouseMoveEvent);
}

//void mouseEnter(QWidget* widget, QPoint localMousePos)
//{
//    QMouseEvent mouseEnterEvent(
//        QEvent::Type::Enter,
//        localMousePos,
//        widget->mapToGlobal(localMousePos),
//        Qt::MouseButton::NoButton,
//        mouseButtons,
//        keyboardModifiers);
//    //QEnterEvent enterEvent()
//    QCursor::setPos(widget->mapToGlobal(localMousePos));
//    QApplication::sendEvent(widget, &mouseEnterEvent);
//}

void ensureSelected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    // 2024-03-01 : Due to the extra "Lighted" hierarchy, it is possible for two different prims to
    // have the same name, only one of which being selected. We will tolerate this in the test, but 
    // we'll make sure there are at most two prims with the same name. We'll also allow a prim not 
    // to have any selections, but there must be at least one prim selected.
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

TEST(TestPicking, pickMesh)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto [argc, argv] = getTestingArgs();
    const std::string primName(argv[0]);

    ensureUnselected(inspector, PrimNamePredicate(primName));

    PrimEntriesVector prims = inspector.FindPrims(MeshPrimPredicate(primName));
    ASSERT_EQ(prims.size(), 1u);

    M3dView active3dView = M3dView::active3dView();

    QPoint primMouseCoords;
    getPrimMouseCoords(prims.front().prim, active3dView, primMouseCoords);

    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), primMouseCoords);

    active3dView.refresh();

    ensureSelected(inspector, PrimNamePredicate(primName));
}

TEST(TestPicking, marqueeSelection)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    const std::string cubePrimName("pCube1");
    const std::string torusPrimName("pTorus1");

    M3dView active3dView = M3dView::active3dView();

    PrimEntriesVector cubeMeshPrims = inspector.FindPrims(MeshPrimPredicate(cubePrimName));
    ASSERT_EQ(cubeMeshPrims.size(), 1u);
    QPoint cubeMouseCoords;
    getPrimMouseCoords(cubeMeshPrims.front().prim, active3dView, cubeMouseCoords);

    PrimEntriesVector torusMeshPrims = inspector.FindPrims(MeshPrimPredicate(torusPrimName));
    ASSERT_EQ(torusMeshPrims.size(), 1u);
    QPoint torusMouseCoords;
    getPrimMouseCoords(torusMeshPrims.front().prim, active3dView, torusMouseCoords);

    ensureUnselected(inspector, PrimNamePredicate(cubePrimName));
    ensureUnselected(inspector, PrimNamePredicate(torusPrimName));

    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), cubeMouseCoords);
    mouseMoveTo(active3dView.widget(), torusMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), torusMouseCoords);

    active3dView.refresh();

    ensureSelected(inspector, PrimNamePredicate(cubePrimName));
    ensureSelected(inspector, PrimNamePredicate(torusPrimName));
}
