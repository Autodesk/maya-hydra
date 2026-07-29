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

#include <flowViewport/selection/fvpPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

#include <mayaHydraLib/adapters/tokens.h>

#include <pxr/imaging/hd/extComputationPrimvarsSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/subdivisionTagsSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <gtest/gtest.h>

#include <QApplication>

#include <algorithm>
#include <sstream>
#include <cctype>
#include <exception>
#include <iostream>
#include <cstring>

namespace {
std::pair<int, char**> testingArgs{0, nullptr};

std::filesystem::path  testInputDir;
std::filesystem::path  testOutputDir;

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

const HdSceneIndexBasePtr GetPassSceneIndex(int passIndex)
{
    const SdfPath passPath = SdfPath("/Pass" + std::to_string(passIndex));
    const auto& sceneIndices = GetTerminalSceneIndices();
    size_t iSceneIndex = 0;
    for (;iSceneIndex < sceneIndices.size(); iSceneIndex++) {
        if (!sceneIndices[iSceneIndex]->GetChildPrimPaths(passPath).empty()) {
            return sceneIndices[iSceneIndex];
        }
    }
    return nullptr;
}

const HdSceneIndexBasePtr GetBeautyPassSceneIndex()
{
    return GetPassSceneIndex(0);
}

const HdSceneIndexBasePtr GetSecondaryGraphicsPassSceneIndex()
{
    return GetPassSceneIndex(1);
}

bool MatricesAreClose(const GfMatrix4d& hydraMatrix, const MMatrix& mayaMatrix, double tolerance)
{
    return GfIsClose(hydraMatrix, GetGfMatrixFromMaya(mayaMatrix), tolerance);
}

FindPrimPredicate CreatePrimPredicate(
    const std::string& primNamePart,
    const TfToken&     primType)
{
    return [primNamePart,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetAsString().find(primNamePart) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == primType;
    };
}

HdSceneIndexBaseRefPtr FindTerminalSceneIndexWithPrim(
    const SceneIndicesVector& sceneIndices,
    FindPrimPredicate         predicate,
    size_t                    maxPrims)
{
    for (const HdSceneIndexBaseRefPtr& sceneIndex : sceneIndices) {
        SceneIndexInspector inspector(sceneIndex);
        PrimEntriesVector   foundPrims = inspector.FindPrims(predicate, maxPrims);
        if (!foundPrims.empty()) {
            return sceneIndex;
        }
    }
    return nullptr;
}

HdSceneIndexBaseRefPtr FindTerminalSceneIndexWithPrim(
    const SceneIndicesVector& sceneIndices,
    const std::string&        primNamePart,
    const TfToken&            primType,
    size_t                    maxPrims)
{
    return FindTerminalSceneIndexWithPrim(
        sceneIndices, CreatePrimPredicate(primNamePart, primType), maxPrims);
}

std::string GetOptionVarOrDefault(const char* optionVar, const char* fallback)
{
    if (MGlobal::optionVarExists(optionVar)) {
        return MGlobal::optionVarStringValue(optionVar).asChar();
    }
    return fallback;
}

std::string GetShapeNameFromFullPath(const std::string& fullPath)
{
    const size_t lastPipe = fullPath.rfind('|');
    if (lastPipe != std::string::npos && lastPipe + 1 < fullPath.size()) {
        return fullPath.substr(lastPipe + 1);
    }
    return fullPath;
}

std::string GetParentNameFromFullPath(const std::string& fullPath)
{
    const size_t lastPipe = fullPath.rfind('|');
    if (lastPipe == std::string::npos || lastPipe == 0) {
        return {};
    }
    const size_t prevPipe = fullPath.rfind('|', lastPipe - 1);
    const size_t start = (prevPipe == std::string::npos) ? 0 : prevPipe + 1;
    if (lastPipe <= start) {
        return {};
    }
    return fullPath.substr(start, lastPipe - start);
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

HdSceneIndexBaseRefPtr FindMayaHydraSceneIndex(
    const HdSceneIndexBaseRefPtr& terminalSceneIndex)
{
    // Two-step lookup is intentional: find the Data Producer Merging Scene Index first,
    // then pick the MayaHydraSceneIndex from its inputs. This anchors the search to the
    // Maya-authored branch of the tree and avoids accidentally selecting a similarly
    // named scene index from another branch if multiple exist.
    auto isDataProducerMerging = SceneIndexDisplayNamePred("Data Producer Merging Scene Index");
    auto mergingSiBase = findSceneIndexInTree(terminalSceneIndex, isDataProducerMerging);
    if (!mergingSiBase) {
        return nullptr;
    }
    auto mergingSi = TfDynamic_cast<HdMergingSceneIndexRefPtr>(mergingSiBase);
    if (!mergingSi) {
        return nullptr;
    }
    auto isMayaProducer = SceneIndexDisplayNamePred("MayaHydraSceneIndex");
    auto producers = mergingSi->GetInputScenes();
    auto found = std::find_if(producers.begin(), producers.end(), isMayaProducer);
    return (found != producers.end()) ? *found : nullptr;
}

MeshDirtySignals ClassifyMeshDirtySince(
    const SceneIndexNotificationsAccumulator& accumulator,
    size_t                                    startIndex,
    const SdfPath&                            meshPrimPath)
{
    MeshDirtySignals signals;
    const auto& entries = accumulator.GetDirtiedPrimEntries();

    const auto meshTopologyLocator = HdMeshTopologySchema::GetDefaultLocator();
    const auto subdivSchemeLocator = HdMeshSchema::GetSubdivisionSchemeLocator();
    const auto broadPrimvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto extCompLocator = HdExtComputationPrimvarsSchema::GetDefaultLocator();
    const auto pointsLocator = HdPrimvarsSchema::GetPointsLocator();
    const auto extentLocator = HdExtentSchema::GetDefaultLocator();
    const auto normalsLocator = HdPrimvarsSchema::GetNormalsLocator();
    const auto uvsLocator = broadPrimvarsLocator.Append(MayaHydraAdapterTokens->st);
    const auto tangentsLocator = broadPrimvarsLocator.Append(MayaHydraAdapterTokens->tangents);
    const auto subdivTagsLocator = HdSubdivisionTagsSchema::GetDefaultLocator();
    const auto displayStyleLocator = HdLegacyDisplayStyleSchema::GetDefaultLocator();
    const auto visibilityLocator = HdVisibilitySchema::GetDefaultLocator();
    const auto instancedByLocator = HdInstancedBySchema::GetDefaultLocator();
    const auto instancerTopologyLocator = HdInstancerTopologySchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != meshPrimPath) {
            continue;
        }
        signals.anyForPrim = true;
        const HdDataSourceLocatorSet& locators = entries[i].dirtyLocators;
        if (locators.Intersects(meshTopologyLocator)
            || locators.Contains(subdivSchemeLocator)) {
            signals.meshTopology = true;
        }
        if (locators.Contains(broadPrimvarsLocator)) {
            signals.broadPrimvars = true;
        }
        if (locators.Intersects(extCompLocator)) {
            signals.extCompPrimvars = true;
        }
        if (locators.Intersects(pointsLocator)) {
            signals.points = true;
        }
        if (locators.Intersects(extentLocator)) {
            signals.extent = true;
        }
        if (locators.Intersects(normalsLocator)) {
            signals.normals = true;
        }
        if (locators.Intersects(uvsLocator)) {
            signals.uvs = true;
        }
        if (locators.Intersects(tangentsLocator)) {
            signals.tangents = true;
        }
        if (locators.Intersects(subdivTagsLocator)) {
            signals.subdivisionTags = true;
        }
        if (locators.Intersects(displayStyleLocator)) {
            signals.displayStyle = true;
        }
        if (locators.Intersects(visibilityLocator)) {
            signals.visibility = true;
        }
        if (locators.Intersects(instancedByLocator)
            || locators.Intersects(instancerTopologyLocator)) {
            signals.instancer = true;
        }
    }
    return signals;
}

std::string DescribeDirtyPrimEntriesSince(
    const SceneIndexNotificationsAccumulator& accumulator,
    size_t                                    startIndex,
    const SdfPath&                            primPath)
{
    std::ostringstream out;
    const auto&        entries = accumulator.GetDirtiedPrimEntries();
    out << "Dirty entries since index " << startIndex;
    if (!primPath.IsEmpty()) {
        out << " for prim " << primPath.GetString();
    }
    out << ":\n";
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (!primPath.IsEmpty() && entries[i].primPath != primPath) {
            continue;
        }
        out << "  [" << i << "] " << entries[i].primPath.GetAsString() << " locators: ";
        const HdDataSourceLocatorSet& locators = entries[i].dirtyLocators;
        if (locators.IsEmpty()) {
            out << "<empty>";
        } else {
            bool first = true;
            for (auto it = locators.begin(); it != locators.end(); ++it) {
                if (!first) {
                    out << ", ";
                }
                first = false;
                out << it->GetString();
            }
        }
        out << "\n";
    }
    return out.str();
}

bool TryFindMeshPrim(
    const std::string&      meshShapeFull,
    SdfPath*                outPrimPath,
    HdSceneIndexBaseRefPtr* outMayaSceneIndex)
{
    if (!outPrimPath || !outMayaSceneIndex) {
        return false;
    }
    *outPrimPath = SdfPath();
    *outMayaSceneIndex = nullptr;

    const std::string         shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    if (sceneIndices.empty()) {
        return false;
    }

    HdSceneIndexBaseRefPtr sceneIndexWithMesh = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    if (!sceneIndexWithMesh) {
        return false;
    }

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithMesh);
    if (!mayaSceneIndex) {
        return false;
    }

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
    if (foundPrims.empty()) {
        return false;
    }

    *outPrimPath = foundPrims.front().primPath;
    *outMayaSceneIndex = mayaSceneIndex;
    return true;
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

std::filesystem::path getInputDir()
{
    if (testInputDir.empty()) {
        throw std::invalid_argument(
            "Attempted to access test input directory but it was not specified.");
    }
    return testInputDir;
}

void setInputDir(std::filesystem::path inputDir)
{ 
    testInputDir = inputDir;
}

std::filesystem::path getOutputDir()
{
    if (testOutputDir.empty()) {
        throw std::invalid_argument(
            "Attempted to access test output directory but it was not specified.");
    }
    return testOutputDir;
}

void setOutputDir(std::filesystem::path outputDir)
{ 
    testOutputDir = outputDir;
}

std::filesystem::path getPathToSample(std::string filename)
{ 
    return getInputDir() / filename;
}

bool dataSourceMatchesReference(
    PXR_NS::HdDataSourceBaseHandle dataSource,
    std::filesystem::path          referencePath)
{
    // We'll dump the data source to a file and then read from it. That way we have a trace
    // of what value was used for comparison, and can inspect it in case of failures.
    std::filesystem::path outputPath = getOutputDir() / referencePath.filename();
    std::fstream          outputFile(outputPath, std::ios::out);
    HdDebugPrintDataSource(outputFile, dataSource);
    outputFile.close();

    outputFile.open(outputPath, std::ios::in);
    std::stringstream outputDump;
    outputDump << outputFile.rdbuf();
    std::string outputString = outputDump.str();

    // Normalize line endings: strip carriage returns for consistent comparison across platforms.
    outputString.erase(
        std::remove(outputString.begin(), outputString.end(), '\r'), outputString.end());

    std::ifstream     referenceFile(referencePath);
    std::stringstream referenceDump;
    referenceDump << referenceFile.rdbuf();
    std::string referenceString = referenceDump.str();

    // Remove carriage returns from the reference string, as these can sometimes be 
    // inadvertently/automatically added to the reference files stored in git.
    // The test outputs always use line feeds only, so no need to do it for those.
    referenceString.erase(
        std::remove(referenceString.begin(), referenceString.end(), '\r'), referenceString.end());

    auto trimTrailingWhitespace = [](std::string& value) {
        auto it = std::find_if_not(
            value.rbegin(),
            value.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        value.erase(it.base(), value.end());
    };
    trimTrailingWhitespace(outputString);
    trimTrailingWhitespace(referenceString);

    // We return a boolean instead of using something like EXPECT_EQ, as that would print the
    // entire dumps to stdout and pollute the logs in case of a test failure. Using EXPECT_TRUE
    // at the callsites still logs exactly which comparison failed, but keeps logs readable.
    return outputString == referenceString;
}

std::filesystem::path getDataSourceComparisonOutputPath(std::filesystem::path referencePath)
{
    return getOutputDir() / referencePath.filename();
}

bool testingArgsEmpty()
{
    // See mayaHydraCppTestsCmd.cpp:constructGoogleTestArgs() documentation.
    auto [argc, argv] = getTestingArgs();
    return (std::strcmp(argv[0], "dummy") == 0);
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

bool visibility(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    auto prim = sceneIndex->GetPrim(primPath);
    auto handle = HdVisibilitySchema::GetFromParent(prim.dataSource).GetVisibility();
    // If there is no handle the prim is visible.
    return (handle ? handle->GetTypedValue(0.0f) : true);
}

bool contains(const PXR_NS::SdfPathVector& paths, const PXR_NS::SdfPath& path)
{
    return std::find(paths.begin(), paths.end(), path) != paths.end();
}

void ensureSelected(const SceneIndexInspector& inspector, const FindPrimPredicate& primPredicate)
{
    // 2024-03-01 : Due to the extra "Lighted" hierarchy, it is possible for an object to be split
    // into two prims, only one of which will be selected. We will tolerate this, but
    // we'll make sure there are at most two prims for that object. We'll also allow a prim not
    // to have any selections, but at least one prim must be selected.
    PrimEntriesVector primEntries = inspector.FindPrims(primPredicate);
    ASSERT_GE(primEntries.size(), 1u);
    ASSERT_LE(primEntries.size(), 2u);

    size_t nbSelectedPrims = 0;
    for (const auto& primEntry : primEntries) {
        PXR_NS::HdSelectionsSchema selectionsSchema
            = PXR_NS::HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        if (selectionsSchema.GetNumElements() > 0u) {
            ASSERT_EQ(selectionsSchema.GetNumElements(), 1u);
            PXR_NS::HdSelectionSchema selectionSchema = selectionsSchema.GetElement(0);
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
        PXR_NS::HdSelectionsSchema selectionsSchema
            = PXR_NS::HdSelectionsSchema::GetFromParent(primEntry.prim.dataSource);
        ASSERT_EQ(selectionsSchema.IsDefined(), false);
    }
}

} // namespace MAYAHYDRA_NS_DEF
