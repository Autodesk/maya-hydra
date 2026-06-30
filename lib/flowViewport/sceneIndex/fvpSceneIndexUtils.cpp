// Copyright 2023 Autodesk
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

#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/version.h>
#include <pxr/imaging/hd/dataSourceLegacyPrim.h>
#if defined(HD_API_VERSION) && HD_API_VERSION >= 74 // For USD 24.11+
#include <pxr/imaging/hd/extComputationSchema.h>
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

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

    outStream << selfPrefix << "@ Prim : " << primPath.MakeRelativePath(primPath.GetParentPath())
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
#if defined(HD_API_VERSION) && HD_API_VERSION >= 74 // For USD 24.11+
        = HdExtComputationCpuCallbackDataSource::Cast(dataSource)) {
#else
        = HdExtComputationCallbackDataSource::Cast(dataSource)) {
#endif
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
    auto filteringSi = TfDynamic_cast<HdFilteringSceneIndexBaseRefPtr>(sceneIndex);
    if (!filteringSi) {
        return {};
    }

    auto sceneIndices = filteringSi->GetInputScenes();
    for (const auto& childSceneIndex : sceneIndices) {
        if (auto si = findSceneIndexInTree(childSceneIndex, predicate)) {
            return si;
        }
    }
    return {};
}

} // namespace FVP_NS_DEF
