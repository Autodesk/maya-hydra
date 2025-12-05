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

#include "fvpNiPrototypeWhSi.h"

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _IsNativePrototype(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) {
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBySchema.IsDefined()) {
        return false;
    }
    SdfPath instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0)[0];
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    return instancerTopologySchema.GetInstanceLocations() != nullptr;
}

}

namespace FVP_NS_DEF {

NiPrototypeWhSiRefPtr NiPrototypeWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new NiPrototypeWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim NiPrototypeWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
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

    HdSelectionsSchema activeSelectionsSchema = HdSelectionsSchema::GetFromParent(GetInputSceneIndex()->GetPrim(selectionKey.first).dataSource);
    HdSelectionSchema activeSelection = activeSelectionsSchema.GetElement(std::stoul(selectionKey.second));

    HdInstanceIndicesSchema instanceIndices = activeSelection.GetNestedInstanceIndices().GetElement(0);
    auto instanceIndex = instanceIndices.GetInstanceIndices()->GetTypedValue(0).front();
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    auto instancerPaths = instancedBySchema.GetPaths();
    if (instancerPaths) {
        auto instancerPath = instancerPaths->GetTypedValue(0).front();
        HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
        HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(instancerPrim.dataSource);
        auto instanceTransformsSchema = primvarsSchema.GetPrimvar(HdInstancerTokens->instanceTransforms);
        auto instanceTransforms = HdTypedSampledDataSource<VtArray<GfMatrix4d>>::Cast(instanceTransformsSchema.GetPrimvarValue());
        auto instanceXform = instanceTransforms->GetTypedValue(0)[instanceIndex];
        auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);
        dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(prototypeXform * instanceXform));
    }

    dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());

    prim.dataSource = dsEditor.Finish();
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(ConvertHydraToFvpSelection(selectionKey.first, activeSelection)));
    }
    prim.dataSource = RepathInstancingDataSources(prim.dataSource, _selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath);
    return prim;
};

SdfPathVector NiPrototypeWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    if (fullPrimPath == selectionPath) {
        // Return only the prototype prim we're interested in
        return {_selectionPathsToPrototypePaths.at(selectionPath)};
    }
    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, _selectionPathsToPrototypePrefixes.at(selectionPath));
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        childPaths.emplace_back(originalChildPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
    }
    return childPaths;
}

NiPrototypeWhSi::NiPrototypeWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (_IsNativePrototype(GetInputSceneIndex(), primPath)) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(primPath, std::to_string(selectionId));
                }
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void NiPrototypeWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (_IsNativePrototype(GetInputSceneIndex(), entry.primPath)) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                }
                // We just created the highlight, no need to add highlight prims
                continue;
            }
        }

        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath), entry.primType);
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void NiPrototypeWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // If a parent of one the selected prims was removed, delete the selection highlight
        auto itSelectedPrim = FindSelfOrFirstChild(entry.primPath, _primPathsToSelections);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            const auto selectionKeysToDelete = itSelectedPrim->second;
            for (const auto& selectionKey : selectionKeysToDelete) {
                _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
            }
        }

        // If a prototype was removed, delete the selection highlights which depended on it
        auto itPrototypeParentRemoval = FindSelfOrFirstChild(entry.primPath, _prototypePathsToSelections);
        if (itPrototypeParentRemoval != _prototypePathsToSelections.end()) {
            const auto selectionKeysToDelete = itPrototypeParentRemoval->second;
            for (const auto& selectionKey : selectionKeysToDelete) {
                _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
            }
        }

        // Propagate removed prototype subprims
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath));
            }
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void NiPrototypeWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsNativePrototype(GetInputSceneIndex(), entry.primPath)) {
                // Selection changed on the prototype; rebuild the highlights
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    const auto selectionKeysToDelete = existingSelectionKeys->second;
                    for (const auto& selectionKey : selectionKeysToDelete) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                    }
                }
                // We rebuilt the highlight, no need to do the rest
                continue;
            }
        }

        // Propagate notifications if this prim is a relevant prototype or a subprim of one
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath), entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void NiPrototypeWhSi::_CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    if (selectionId.empty() || selectionId.find_first_not_of("0123456789") != std::string::npos) {
        return;
    }

    // Setup data structures
    SelectionKey selectionKey { prototypePath, selectionId };
    SdfPath selectionPath = RegisterSelection(selectionKey);

    _prototypePathsToSelections[prototypePath].emplace(selectionKey);
    _selectionPathsToPrototypePrefixes.emplace(selectionPath, prototypePath.GetParentPath());
    _selectionPathsToPrototypePaths.emplace(selectionPath, prototypePath);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, prototypePath, selectionPath](const PXR_NS::SdfPath& primPath, const PXR_NS::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(prototypePath.GetParentPath(), selectionPath), prim.primType);
        return true;
    };
    ForEachPrimInHierarchy(prototypePath, operation);
    _SendPrimsAdded(addedPrims);
}

void NiPrototypeWhSi::_DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Erase from data structures
    SelectionKey selectionKey { prototypePath, selectionId };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    auto prototypePrefix = _selectionPathsToPrototypePrefixes.at(selectionPath);
    auto itPrototypePath = _prototypePathsToSelections.upper_bound(prototypePrefix);
    if (itPrototypePath != _prototypePathsToSelections.end()) {
        itPrototypePath->second.erase(selectionKey);
    }
    _selectionPathsToPrototypePrefixes.erase(selectionPath);
    _selectionPathsToPrototypePaths.erase(selectionPath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
