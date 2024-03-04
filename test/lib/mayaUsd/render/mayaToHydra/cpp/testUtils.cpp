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
#include <maya/MSelectionList.h>

#include <exception>
#include <iostream>

namespace {
std::pair<int, char**> testingArgs{0, nullptr};

std::filesystem::path  testInputDir;
std::filesystem::path  testOutputDir;
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

PXR_NAMESPACE_CLOSE_SCOPE

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

    std::ifstream     referenceFile(referencePath);
    std::stringstream referenceDump;
    referenceDump << referenceFile.rdbuf();

    // We return a boolean instead of using something like EXPECT_EQ, as that would print the
    // entire dumps to stdout and pollute the logs in case of a test failure. Using EXPECT_TRUE
    // at the callsites still logs exactly which comparison failed, but keeps logs readable.
    return outputDump.str() == referenceDump.str();
}

} // namespace MAYAHYDRA_NS_DEF
