// Copyright 2025 Autodesk
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

#include "fvpNiInstanceWhSi.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

SdfPath _GetNativeInstancePrototypePath(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& nativeInstancePrimPath) {
    HdSceneIndexPrim nativeInstancePrim = sceneIndex->GetPrim(nativeInstancePrimPath);
    HdInstanceSchema instanceSchema = HdInstanceSchema::GetFromParent(nativeInstancePrim.dataSource);
    auto instancerPath = instanceSchema.GetInstancer()->GetTypedValue(0);
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    auto prototypePath = instancerTopologySchema.GetPrototypes()->GetTypedValue(0)[instanceSchema.GetPrototypeIndex()->GetTypedValue(0)];
    return prototypePath;
}

}

namespace FVP_NS_DEF {

NiInstanceWhSiRefPtr NiInstanceWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new NiInstanceWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim NiInstanceWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    if (!prim.dataSource) {
        // If there is no data source, return. Trying to get child
        // data sources or schemas from a null data source will crash.
        return prim;
    }
    HdContainerDataSourceEditor dsEditor(prim.dataSource);

    dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());

    HdSceneIndexPrim instancePrim = GetInputSceneIndex()->GetPrim(selectionKey.first);
    auto instanceXform = HdXformSchema::GetFromParent(instancePrim.dataSource).GetMatrix()->GetTypedValue(0);
    auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);
    dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(prototypeXform * instanceXform));

    prim.dataSource = dsEditor.Finish();
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
    prim.dataSource = RepathInstancingDataSources(prim.dataSource, _selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath);
    return prim;
};

SdfPathVector NiInstanceWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    SdfPath prototypePath = _GetNativeInstancePrototypePath(GetInputSceneIndex(), selectionKey.first);

    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        // Only return paths under and including the selected instance's prototype
        if (originalChildPath.HasPrefix(prototypePath)) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
        }
    }
    return childPaths;
}

NiInstanceWhSi::NiInstanceWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        HdInstanceSchema instance = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instance.IsDefined()) {
            _instancePaths.emplace(primPath);
            if (HasFullySelectedAncestorInclusive(primPath)) {
                _CreateSelectionHighlight(primPath);
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void NiInstanceWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        HdInstanceSchema instance = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instance.IsDefined()) {
            _instancePaths.emplace(entry.primPath);
            if (HasFullySelectedAncestorInclusive(entry.primPath)) {
                _CreateSelectionHighlight(entry.primPath);
            }
        }

        // Propagate added prototype subprims
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelectionPaths);
        if (itPrototype != _prototypePathsToSelectionPaths.end()) {
            for (const auto& selectionPath : itPrototype->second) {
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath), entry.primType);
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void NiInstanceWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // Delete all selection highlights for instances rooted under the removed prim
        auto itInstance = FindSelfOrFirstChild(entry.primPath, _instancePaths);
        while (itInstance != _instancePaths.end() && itInstance->HasPrefix(entry.primPath)) {
            _DeleteSelectionHighlight(*itInstance);
            itInstance = _instancePaths.erase(itInstance);
        }

        // If a prototype was removed, delete the selection highlights which depended on it
        auto itPrototypeParentRemoval = FindSelfOrFirstChild(entry.primPath, _prototypePathsToSelectionPaths);
        if (itPrototypeParentRemoval != _prototypePathsToSelectionPaths.end()) {
            const auto selectionPathsToDelete = itPrototypeParentRemoval->second;
            for (const auto& selectionPath : selectionPathsToDelete) {
                _DeleteSelectionHighlight(SelectionKeyFromPath(selectionPath).first);
            }
        }

        // Propagate removed prototype subprims
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelectionPaths);
        if (itPrototype != _prototypePathsToSelectionPaths.end()) {
            for (const auto& selectionPath : itPrototype->second) {
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath));
            }
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void NiInstanceWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // If instance structure was dirtied, rebuild the highlight
        if (_primPathsToSelections.find(entry.primPath) != _primPathsToSelections.end()
            && entry.dirtyLocators.Intersects(HdInstanceSchema::GetDefaultLocator())) {
            _DeleteSelectionHighlight(entry.primPath);
            _CreateSelectionHighlight(entry.primPath);
        }
        
        // Propagate notifications to the prototype highlight if the dirtied prim is a relevant prototype or a subprim of one
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelectionPaths);
        if (itPrototype != _prototypePathsToSelectionPaths.end()) {
            for (const auto& selectionPath : itPrototype->second) {
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath), entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void NiInstanceWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    if (isFullySelected) {
        for (auto itInstance = FindSelfOrFirstChild(primPath, _instancePaths); itInstance != _instancePaths.end() && itInstance->HasPrefix(primPath); itInstance++) {
            _CreateSelectionHighlight(*itInstance);
        }
    }
    else {
        for (auto itInstance = FindSelfOrFirstChild(primPath, _instancePaths); itInstance != _instancePaths.end() && itInstance->HasPrefix(primPath); itInstance++) {
            if (!HasFullySelectedAncestorInclusive(*itInstance)) {
                _DeleteSelectionHighlight(*itInstance);
            }
        }
    }
}

void NiInstanceWhSi::_CreateSelectionHighlight(const SdfPath& instancePath)
{
    if (_primPathsToSelections.find(instancePath) != _primPathsToSelections.end()) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { instancePath, "" };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    HdSceneIndexPrim instancePrim = GetInputSceneIndex()->GetPrim(instancePath);
    HdInstanceSchema instance = HdInstanceSchema::GetFromParent(instancePrim.dataSource);
    auto instancerPath = instance.GetInstancer()->GetTypedValue(0);
    HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    auto prototypePath = instancerTopology.GetPrototypes()->GetTypedValue(0)[instance.GetPrototypeIndex()->GetTypedValue(0)];
    _prototypePathsToSelectionPaths[prototypePath].emplace(selectionPath);
    _selectionPathsToPrototypePrefixes.emplace(selectionPath, prototypePath.GetParentPath());

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, prototypePath, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(prototypePath.GetParentPath(), selectionPath), prim.primType);
        return true;
    };
    ForEachPrimInHierarchy(prototypePath, operation);
    _SendPrimsAdded(addedPrims);
}

void NiInstanceWhSi::_DeleteSelectionHighlight(const SdfPath& instancePath)
{
    if (_primPathsToSelections.find(instancePath) == _primPathsToSelections.end()) {
        return;
    }

    // Erase from data structures
    SelectionKey selectionKey { instancePath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    auto prototypePrefix = _selectionPathsToPrototypePrefixes.at(selectionPath);
    auto itPrototypePath = _prototypePathsToSelectionPaths.upper_bound(prototypePrefix);
    if (itPrototypePath != _prototypePathsToSelectionPaths.end()) {
        itPrototypePath->second.erase(selectionPath);
    }
    _selectionPathsToPrototypePrefixes.erase(selectionPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
