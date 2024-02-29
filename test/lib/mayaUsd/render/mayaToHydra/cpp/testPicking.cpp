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

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/M3dView.h>
#include <maya/MGlobal.h>
#include <maya/MSelectionList.h>
#include <maya/MFnTransform.h>
#include <maya/MDagPath.h>

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

void getMouseCoords(std::string nodeName, M3dView& view, QPoint& outMouseCoords)
{
    MPoint worldPosition;
    ASSERT_TRUE(GetWorldPositionFromNodeName(MString(nodeName.c_str()), worldPosition));
    short mouseX = 0, mouseY = 0;
    MStatus worldToViewStatus;
    // The first assert checks that the point was not clipped, the second checks the general MStatus
    ASSERT_TRUE(view.worldToView(worldPosition, mouseX, mouseY, &worldToViewStatus));
    ASSERT_TRUE(worldToViewStatus);
    // Qt and M3dView use opposite Y-coordinates
    outMouseCoords = QPoint(mouseX, view.portHeight() - mouseY);
}

void getMouseCoords2(const SceneIndexInspector& sceneIndexInspector, const FindPrimPredicate& primPredicate, M3dView& view, QPoint& outMouseCoords)
{
    PrimEntriesVector prims = sceneIndexInspector.FindPrims(primPredicate);
    ASSERT_EQ(prims.size(), 1u);

    HdDataSourceBaseHandle xformDataSource = HdContainerDataSource::Get(prims.front().prim.dataSource, HdXformSchema::GetDefaultLocator());
    ASSERT_NE(xformDataSource, nullptr);
    HdContainerDataSourceHandle xformContainerDataSource = HdContainerDataSource::Cast(xformDataSource);
    ASSERT_NE(xformContainerDataSource, nullptr);
    HdXformSchema xformSchema(xformContainerDataSource);
    ASSERT_NE(xformSchema.GetMatrix(), nullptr);
    GfMatrix4d xformMatrix = xformSchema.GetMatrix()->GetTypedValue(0);
    GfVec3d translation = xformMatrix.ExtractTranslation();

    MPoint worldPosition(translation.data());
    short   mouseX = 0, mouseY = 0;
    MStatus worldToViewStatus;
    // The first assert checks that the point was not clipped, the second checks the general MStatus
    ASSERT_TRUE(view.worldToView(worldPosition, mouseX, mouseY, &worldToViewStatus));
    ASSERT_TRUE(worldToViewStatus);

    // Qt and M3dView use opposite Y-coordinates
    outMouseCoords = QPoint(mouseX, view.portHeight() - mouseY);
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

} // namespace

TEST(TestPicking, clickAndRelease)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto active3dView = M3dView::active3dView();
    auto active3dViewWidget = active3dView.widget();
    std::cout << "DBP-DBG (active3dViewWidget) : "
              << active3dViewWidget->objectName().toStdString()
              << std::endl;
    active3dViewWidget->dumpObjectInfo();
    active3dViewWidget->dumpObjectTree();
    std::cout << "QWidget dims : " << active3dViewWidget->width() << " x " << active3dViewWidget->height();
    std::cout << "M3dView port dims : " << active3dView.portWidth() << " x " << active3dView.portHeight();
    std::cout << "M3dView playblast dims : " << active3dView.playblastPortWidth() << " x " << active3dView.playblastPortHeight();
    
    QPoint mousePos = QPoint(active3dViewWidget->width() / 2, active3dViewWidget->height() / 2);
    QCursor::setPos(active3dViewWidget->mapToGlobal(mousePos));
    QMouseEvent mousePressEvent(
        QEvent::MouseButtonPress,
        mousePos,
        active3dViewWidget->mapToGlobal(mousePos),
        Qt::MouseButton::LeftButton,
        Qt::MouseButton::NoButton,
        Qt::KeyboardModifier::NoModifier);
    QApplication::sendEvent(active3dViewWidget, &mousePressEvent);
    QMouseEvent mouseReleaseEvent(
        QEvent::MouseButtonRelease,
        mousePos,
        active3dViewWidget->mapToGlobal(mousePos),
        Qt::MouseButton::LeftButton,
        Qt::MouseButton::NoButton,
        Qt::KeyboardModifier::NoModifier);
    QApplication::sendEvent(active3dViewWidget, &mouseReleaseEvent);


    /*QApplication::postEvent(active3dViewWidget, &mouseReleaseEvent);
    QApplication::processEvents(QEventLoop::ProcessEventsFlag::EventLoopExec);
    QCoreApplication* app = QApplication::instance();
    app->notify(active3dViewWidget, &mouseReleaseEvent);*/
}

TEST(TestPicking, clickAndReleaseMarquee)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    M3dView active3dView = M3dView::active3dView();

    QPoint cubeMouseCoords;
    //getMouseCoords2(inspector, RenderItemMeshPrimPredicate("pCubeShape1"), active3dView, cubeMouseCoords);
    getMouseCoords("pCube1", active3dView, cubeMouseCoords);

    QPoint torusMouseCoords;
    //getMouseCoords2(inspector, RenderItemMeshPrimPredicate("pTorusShape1"), active3dView, torusMouseCoords);
    getMouseCoords("pTorus1", active3dView, torusMouseCoords);

    mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), cubeMouseCoords);
    mouseMoveTo(active3dView.widget(), torusMouseCoords);
    mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), torusMouseCoords);

    //mousePress(Qt::MouseButton::LeftButton, active3dView.widget(), cubeMouseCoords);
    //mouseRelease(Qt::MouseButton::LeftButton, active3dView.widget(), torusMouseCoords);

        
    /*MPoint worldPos;
    ASSERT_TRUE(getPosition(MString("pCube1"), worldPos));
    short                                   x, y;
    active3dView.worldToView(worldPos, x, y);
    auto active3dViewWidget = active3dView.widget();*/
    //active3dViewWidget->parentWidget()->parentWidget()->parentWidget()->resize(1000, 500);
    /*std::cout << "DBP-DBG (active3dViewWidget) : " << active3dViewWidget->objectName().toStdString()
              << std::endl;
    active3dViewWidget->dumpObjectInfo();
    active3dViewWidget->dumpObjectTree();
    std::cout << "QWidget dims : " << active3dViewWidget->width() << " x "
              << active3dViewWidget->height();
    std::cout << "M3dView port dims : " << active3dView.portWidth() << " x "
              << active3dView.portHeight();
    std::cout << "M3dView playblast dims : " << active3dView.playblastPortWidth() << " x "
              << active3dView.playblastPortHeight();*/

    /*QWidget* parent = active3dViewWidget->parentWidget();
    while (parent) {
        std::cout << "DBP-DBG (parent) : " << parent->objectName().toStdString()
                  << std::endl;
        parent->dumpObjectInfo();
        parent->dumpObjectTree();
        std::cout << "QWidget dims : " << parent->width() << " x " << parent->height();
        parent = parent->parentWidget();
    }*/

    //QPoint mousePos = QPoint(active3dViewWidget->width() / 2 - 250, active3dViewWidget->height() / 2);
    //QPoint mousePos = QPoint(x, active3dView.portHeight() - y);
    //click(active3dViewWidget, mousePos);
    /*QCursor::setPos(active3dViewWidget->mapToGlobal(mousePos));
    QMouseEvent mousePressEvent(
        QEvent::MouseButtonPress,
        mousePos,
        active3dViewWidget->mapToGlobal(mousePos),
        Qt::MouseButton::LeftButton,
        Qt::MouseButton::NoButton,
        Qt::KeyboardModifier::NoModifier);
    QApplication::sendEvent(active3dViewWidget, &mousePressEvent);
    QMouseEvent mouseMoveEvent(
        QEvent::MouseMove,
        mousePos,
        active3dViewWidget->mapToGlobal(mousePos),
        Qt::MouseButton::LeftButton,
        Qt::MouseButton::NoButton,
        Qt::KeyboardModifier::NoModifier
    )
    QMouseEvent mouseReleaseEvent(
        QEvent::MouseButtonRelease,
        mousePos,
        active3dViewWidget->mapToGlobal(mousePos),
        Qt::MouseButton::LeftButton,
        Qt::MouseButton::NoButton,
        Qt::KeyboardModifier::NoModifier);
    QApplication::sendEvent(active3dViewWidget, &mouseReleaseEvent);*/

    /*QApplication::postEvent(active3dViewWidget, &mouseReleaseEvent);
    QApplication::processEvents(QEventLoop::ProcessEventsFlag::EventLoopExec);
    QCoreApplication* app = QApplication::instance();
    app->notify(active3dViewWidget, &mouseReleaseEvent);*/
}
