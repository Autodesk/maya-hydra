//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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
#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/mixedUtils.h>

#include <pxr/imaging/hd/dataSourceLegacyPrim.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include <gtest/gtest.h>

#include <QApplication>

#include <iostream>

namespace {
std::pair<int, char**> testingArgs{0, nullptr};

// Store the ongoing state of the pressed moused & keyboard buttons.
// These are normally kept track of internally by Qt and can be retrieved using 
// methods of the same name. But since we are sending artificial events, Qt does 
// not get the opportunity to set these, so we keep track of them manually here.
Qt::MouseButtons      mouseButtons;
Qt::KeyboardModifiers keyboardModifiers;
}

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

const SceneIndicesVector& GetTerminalSceneIndices()
{
    return GetMayaHydraLibInterface().GetTerminalSceneIndices();
}

bool MatricesAreClose(const GfMatrix4d& hydraMatrix, const MMatrix& mayaMatrix, double tolerance)
{
    return GfIsClose(hydraMatrix, GetGfMatrixFromMaya(mayaMatrix), tolerance);
}

SceneIndexInspector::SceneIndexInspector(HdSceneIndexBasePtr sceneIndex)
    : _sceneIndex(sceneIndex)
{
}

SceneIndexInspector::~SceneIndexInspector() { }

HdSceneIndexBasePtr SceneIndexInspector::GetSceneIndex() { return _sceneIndex; }

PrimEntriesVector SceneIndexInspector::FindPrims(FindPrimPredicate predicate, size_t maxPrims) const
{
    PrimEntriesVector searchResults;
    _FindPrims(predicate, SdfPath::AbsoluteRootPath(), searchResults, maxPrims);
    return searchResults;
}

void SceneIndexInspector::_FindPrims(
    FindPrimPredicate  predicate,
    const SdfPath&     primPath,
    PrimEntriesVector& primEntries,
    size_t             maxPrims) const
{
    HdSceneIndexPrim prim = _sceneIndex->GetPrim(primPath);
    if (predicate(_sceneIndex, primPath)) {
        primEntries.push_back({ primPath, prim });
        if (maxPrims > 0 && primEntries.size() >= maxPrims) {
            return;
        }
    } else {
        auto childPaths = _sceneIndex->GetChildPrimPaths(primPath);
        for (auto childPath : childPaths) {
            _FindPrims(predicate, childPath, primEntries, maxPrims);
            if (maxPrims > 0 && primEntries.size() >= maxPrims) {
                return;
            }
        }
    }
}

void SceneIndexInspector::WriteHierarchy(std::ostream& outStream) const
{
    _WritePrimHierarchy(SdfPath::AbsoluteRootPath(), "", "", outStream);
}

void SceneIndexInspector::_WritePrimHierarchy(
    SdfPath       primPath,
    std::string   selfPrefix,
    std::string   childrenPrefix,
    std::ostream& outStream) const
{
    HdSceneIndexPrim prim = _sceneIndex->GetPrim(primPath);

    outStream << selfPrefix << "@ Prim : " << MakeRelativeToParentPath(primPath)
              << " --- Type : " << prim.primType.GetString() << "\n";

    _WriteContainerDataSource(
        prim.dataSource, "", childrenPrefix + "|___", childrenPrefix + "    ", outStream);

    auto childPaths = _sceneIndex->GetChildPrimPaths(primPath);
    for (auto childPath : childPaths) {
        bool isLastChild = childPath == childPaths.back();
        _WritePrimHierarchy(
            childPath,
            childrenPrefix + "|___",
            childrenPrefix + (isLastChild ? "    " : "|   "),
            outStream);
    }
}

void SceneIndexInspector::_WriteContainerDataSource(
    HdContainerDataSourceHandle dataSource,
    std::string                 dataSourceName,
    std::string                 selfPrefix,
    std::string                 childrenPrefix,
    std::ostream&               outStream) const
{
    if (!dataSource) {
        return;
    }

    outStream << selfPrefix << "# ContainerDataSource : " << dataSourceName << "\n";

    auto childNames = dataSource->GetNames();
    for (auto childName : childNames) {
        bool isLastChild = childName == childNames.back();
        auto child = dataSource->Get(childName);
        if (auto childContainer = HdContainerDataSource::Cast(child)) {
            _WriteContainerDataSource(
                childContainer,
                childName.GetString(),
                childrenPrefix + "|___",
                childrenPrefix + (isLastChild ? "    " : "|   "),
                outStream);
        } else if (auto childVector = HdVectorDataSource::Cast(child)) {
            _WriteVectorDataSource(
                childVector,
                childName.GetString(),
                childrenPrefix + "|___",
                childrenPrefix + (isLastChild ? "    " : "|   "),
                outStream);
        } else {
            _WriteLeafDataSource(child, childName, childrenPrefix + "|___", outStream);
        }
    }
}

void SceneIndexInspector::_WriteVectorDataSource(
    HdVectorDataSourceHandle dataSource,
    std::string              dataSourceName,
    std::string              selfPrefix,
    std::string              childrenPrefix,
    std::ostream&            outStream) const
{
    if (!dataSource) {
        return;
    }

    outStream << selfPrefix << "# VectorDataSource : " << dataSourceName << "\n";

    auto numElements = dataSource->GetNumElements();
    for (size_t iElement = 0; iElement < numElements; iElement++) {
        std::string childName = "Element " + std::to_string(iElement);
        bool        isLastElement = iElement == numElements - 1;
        auto        child = dataSource->GetElement(iElement);
        if (auto childContainer = HdContainerDataSource::Cast(child)) {
            _WriteContainerDataSource(
                childContainer,
                childName,
                childrenPrefix + "|___",
                childrenPrefix + (isLastElement ? "    " : "|   "),
                outStream);
        } else if (auto childVector = HdVectorDataSource::Cast(child)) {
            _WriteVectorDataSource(
                childVector,
                childName,
                childrenPrefix + "|___",
                childrenPrefix + (isLastElement ? "    " : "|   "),
                outStream);
        } else {
            _WriteLeafDataSource(child, childName, childrenPrefix + "|___", outStream);
        }
    }
}

void SceneIndexInspector::_WriteLeafDataSource(
    HdDataSourceBaseHandle dataSource,
    std::string            dataSourceName,
    std::string            selfPrefix,
    std::ostream&          outStream) const
{
    std::string dataSourceDescription;
    if (auto blockDataSource = HdBlockDataSource::Cast(dataSource)) {
        dataSourceDescription = "BlockDataSource";
    } else if (auto sampledDataSource = HdSampledDataSource::Cast(dataSource)) {
        dataSourceDescription
            = "SampledDataSource -> " + sampledDataSource->GetValue(0).GetTypeName();
    } else if (
        auto extComputationCallbackDataSource
        = HdExtComputationCallbackDataSource::Cast(dataSource)) {
        dataSourceDescription = "ExtComputationCallbackDataSource";
    } else {
        dataSourceDescription = "Unidentified data source type";
    }
    outStream << selfPrefix << dataSourceDescription << " : " << dataSourceName << "\n";
}

HdSceneIndexBaseRefPtr findSceneIndexInTree(
    const HdSceneIndexBaseRefPtr&                             sceneIndex,
    const std::function<bool(const HdSceneIndexBaseRefPtr&)>& predicate
)
{
    if (predicate(sceneIndex)) {
        return sceneIndex;
    }
    auto filteringSi = TfDynamic_cast<HdFilteringSceneIndexBaseRefPtr>(
        sceneIndex);
    // End recursion at leaf scene indices, which are not filtering scene
    // indices.
    if (!filteringSi) {
        return {};
    }

    auto sceneIndices = filteringSi->GetInputScenes();
    if (!sceneIndices.empty()) {
        for (const auto& childSceneIndex : sceneIndices) {
            if (auto si = findSceneIndexInTree(childSceneIndex, predicate)) {
                return si;
            }
        }
    }
    return {};
}

Fvp::SelectionSceneIndexRefPtr findSelectionSceneIndexInTree(
    const HdSceneIndexBaseRefPtr& sceneIndex
)
{
    auto isFvpSelectionSceneIndex = SceneIndexDisplayNamePred(
        "Flow Viewport Selection Scene Index");
    auto selectionSiBase = findSceneIndexInTree(
        sceneIndex, isFvpSelectionSceneIndex);
    return TfDynamic_cast<Fvp::SelectionSceneIndexRefPtr>(selectionSiBase);
}

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

void setTestingArgs(int argc, char** argv)
{
    testingArgs = {argc, argv};
}

std::pair<int, char**> getTestingArgs()
{
    return testingArgs;
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

    QApplication::sendEvent(widget, &mouseMoveEvent);
}

void mousePress(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos)
{
    QMouseEvent mousePressEvent(
        QEvent::Type::MouseButtonPress,
        localMousePos,
        widget->mapToGlobal(localMousePos),
        mouseButton,
        mouseButtons,
        keyboardModifiers);

    // Update mouse state
    mouseButtons |= mouseButton;

    QApplication::sendEvent(widget, &mousePressEvent);
}

void mouseRelease(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos)
{
    // Update mouse state
    mouseButtons &= ~mouseButton;

    QMouseEvent mouseReleaseEvent(
        QEvent::Type::MouseButtonRelease,
        localMousePos,
        widget->mapToGlobal(localMousePos),
        mouseButton,
        mouseButtons,
        keyboardModifiers);

    QApplication::sendEvent(widget, &mouseReleaseEvent);
}

void mouseClick(Qt::MouseButton mouseButton, QWidget* widget, QPoint localMousePos)
{
    mousePress(mouseButton, widget, localMousePos);
    mouseRelease(mouseButton, widget, localMousePos);
}

QPoint getPrimMouseCoords(const HdSceneIndexPrim& prim, M3dView& view)
{
    HdDataSourceBaseHandle xformDataSource = HdContainerDataSource::Get(prim.dataSource, HdXformSchema::GetDefaultLocator());
    if (!xformDataSource) {
        ADD_FAILURE() << "Scene index prim has no default locator data source, cannot get mouse coordinates for it.";
        return {};
    }
    HdContainerDataSourceHandle xformContainerDataSource = HdContainerDataSource::Cast(xformDataSource);
    TF_AXIOM(xformContainerDataSource);
    HdXformSchema xformSchema(xformContainerDataSource);
    TF_AXIOM(xformSchema.GetMatrix());
    GfMatrix4d xformMatrix = xformSchema.GetMatrix()->GetTypedValue(0);
    GfVec3d translation = xformMatrix.ExtractTranslation();

    MPoint worldPosition(translation[0], translation[1], translation[2], 1.0);
    short   viewportX = 0, viewportY = 0;
    MStatus worldToViewStatus;
    // First assert checks that the point was not clipped, second assert checks the general MStatus
    if (!view.worldToView(worldPosition, viewportX, viewportY, &worldToViewStatus)) {
        ADD_FAILURE() << "point was clipped by world to view projection, cannot get mouse coordinates for scene index prim.";
        return {};
    }
    if (worldToViewStatus != MS::kSuccess) {
        ADD_FAILURE() << "M3dView::worldToView() failed, cannot get mouse coordinates for scene index prim.";
        return {};
    }

    // Qt and M3dView use opposite Y-coordinates
    return QPoint(viewportX, view.portHeight() - viewportY);
}

}
