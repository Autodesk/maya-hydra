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

#include "fvpPiPrototypeWhSi.h"

#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const std::set<TfToken> kSupportedPrimTypes = {
    HdPrimTypeTokens->mesh,
#if PXR_VERSION >= 2405
    HdPrimTypeTokens->geomSubset,
#endif
};

bool _IsSupportedPointInstancePrototype(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) {
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    if (kSupportedPrimTypes.find(prim.primType) == kSupportedPrimTypes.cend()) {
        return false;
    }
    HdInstancedBySchema instancedBySchema = HdInstancedBySchema::GetFromParent(prim.dataSource);
    if (!instancedBySchema.IsDefined()) {
        return false;
    }
    SdfPath instancerPath = instancedBySchema.GetPaths()->GetTypedValue(0)[0];
    HdSceneIndexPrim instancerPrim = sceneIndex->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    return instancerTopologySchema.GetInstanceLocations() == nullptr;
}

}

namespace FVP_NS_DEF {

PiPrototypeWhSiRefPtr PiPrototypeWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PiPrototypeWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim PiPrototypeWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    HdSceneIndexPrim originalPrototypePrim = GetInputSceneIndex()->GetPrim(selectionKey.first);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
#if PXR_VERSION >= 2405
        if (originalPrototypePrim.primType == HdPrimTypeTokens->geomSubset && originalPath == selectionKey.first.GetParentPath()) {
            prim.dataSource = MakeGeomSubsetHighlight(prim.dataSource, originalPrototypePrim.dataSource);
        }
#endif
    }
    prim.dataSource = RepathInstancingDataSources(prim.dataSource, SdfPath::AbsoluteRootPath(), selectionPath);
    return prim;
};

SdfPathVector PiPrototypeWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    HdSceneIndexPrim originalPrototypePrim = GetInputSceneIndex()->GetPrim(selectionKey.first);

    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        bool isInstancerRelevantPath =
            FindSelfOrFirstParent(originalChildPath, _instancerPathsToSelections) != _instancerPathsToSelections.end()
            || FindSelfOrFirstChild(originalChildPath, _instancerPathsToSelections) != _instancerPathsToSelections.end();
        bool isPrototypeRelevantPath =
            FindSelfOrFirstParent(originalChildPath, _prototypePathsToSelections) != _prototypePathsToSelections.end()
            || FindSelfOrFirstChild(originalChildPath, _prototypePathsToSelections) != _prototypePathsToSelections.end();
        bool isRelevantPath = isInstancerRelevantPath || isPrototypeRelevantPath;
        if (isRelevantPath) {
#if PXR_VERSION >= 2405
            bool isSelectedGeomSubsetPrototypePath = originalPrototypePrim.primType == HdPrimTypeTokens->geomSubset && originalChildPath == selectionKey.first;
            if (!isSelectedGeomSubsetPrototypePath) {
                childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
            }
#else
            childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
#endif
        }
    }
    return childPaths;
}

PiPrototypeWhSi::PiPrototypeWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (_IsSupportedPointInstancePrototype(GetInputSceneIndex(), primPath)) {
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

void PiPrototypeWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (_IsSupportedPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
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

        // Propagate added prims notifications for prims rooted under a relevant instancer
        auto itInstancer = FindSelfOrFirstParent(entry.primPath, _instancerPathsToSelections);
        if (itInstancer != _instancerPathsToSelections.end()) {
            for (const auto& selectionKey : itInstancer->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
            }
        }

        // Propagate added prims notifications for prims rooted under a relevant prototype
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
            }
        }
    }
    _SendPrimsAdded(highlightEntries);
}

void PiPrototypeWhSi::ProcessRemovedPrims(
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

        // If an instancer was removed, delete the selection highlights which depended on it
        auto itInstancerParentRemoval = FindSelfOrFirstChild(entry.primPath, _instancerPathsToSelections);
        if (itInstancerParentRemoval != _instancerPathsToSelections.end()) {
            const auto selectionKeysToDelete = itInstancerParentRemoval->second;
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

        // Propagate removed prims notifications for prims rooted under a relevant instancer
        auto itInstancer = FindSelfOrFirstParent(entry.primPath, _instancerPathsToSelections);
        if (itInstancer != _instancerPathsToSelections.end()) {
            for (const auto& selectionKey : itInstancer->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
            }
        }

        // Propagate removed prims notifications for prims rooted under a relevant prototype
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
            }
        }
    }
    _SendPrimsRemoved(highlightEntries);
}

void PiPrototypeWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            if (_IsSupportedPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
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

        if ((_instancerPathsToSelections.find(entry.primPath) != _instancerPathsToSelections.end() || _prototypePathsToSelections.find(entry.primPath) != _prototypePathsToSelections.end())
            && entry.dirtyLocators.Intersects(HdInstancerTopologySchema::GetDefaultLocator())) {
            // Instancing topology was changed : rebuild the highlights, since we don't know exactly how it was changed
            auto instancerSelectionKeysToRebuild = _instancerPathsToSelections.find(entry.primPath);
            if (instancerSelectionKeysToRebuild != _instancerPathsToSelections.end()) {
                const auto selectionKeysToRebuild = instancerSelectionKeysToRebuild->second;
                for (const auto& selectionKey : selectionKeysToRebuild) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            auto prototypeSelectionKeysToRebuild = _prototypePathsToSelections.find(entry.primPath);
            if (prototypeSelectionKeysToRebuild != _prototypePathsToSelections.end()) {
                const auto selectionKeysToRebuild = prototypeSelectionKeysToRebuild->second;
                for (const auto& selectionKey : selectionKeysToRebuild) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            // No need to dirty in this case since we'll have removed and re-added prims, skip to next entries
            continue;
        }

        // Propagate notifications if this prim is a relevant instancer or a subprim of one
        auto itInstancer = FindSelfOrFirstParent(entry.primPath, _instancerPathsToSelections);
        if (itInstancer != _instancerPathsToSelections.end()) {
            for (const auto& selectionKey : itInstancer->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
            }
        }

        // Propagate notifications if this prim is a relevant prototype or a subprim of one
        auto itPrototype = FindSelfOrFirstParent(entry.primPath, _prototypePathsToSelections);
        if (itPrototype != _prototypePathsToSelections.end()) {
            for (const auto& selectionKey : itPrototype->second) {
                auto selectionPath = SelectionPathFromKey(selectionKey);
                auto dirtiedPath = entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath);
#if PXR_VERSION >= 2405
                if (prim.primType == HdPrimTypeTokens->geomSubset && entry.primPath == selectionKey.first) {
                    dirtiedPath = dirtiedPath.GetParentPath();
                }
#endif
                highlightEntries.emplace_back(dirtiedPath, entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void PiPrototypeWhSi::_CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    if (selectionId.empty() || selectionId.find_first_not_of("0123456789") != std::string::npos) {
        return;
    }

    SelectionKey selectionKey { prototypePath, selectionId };
    if (_selections.find(selectionKey) != _selections.end()) {
        return;
    }

    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    CollectInstancingPaths(prototypePath, InstancingPathsCollectionDirection::Bidirectional, instancerPaths, prototypePaths);

    // Setup data structures
    SdfPath selectionPath = RegisterSelection(selectionKey);

    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(prototypePath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    SelectionData selectionData;
    selectionData._primSelection = ConvertHydraToFvpSelection(prototypePath, selectionsSchema.GetElement(std::stoul(selectionId)));
    selectionData._instancerPaths = instancerPaths;
    selectionData._prototypePaths = prototypePaths;
    _selections[selectionKey] = selectionData;
    for (const auto& instancerPath : instancerPaths) {
        _instancerPathsToSelections[instancerPath].emplace(selectionKey);
    }
    for (const auto& prototypePath : prototypePaths) {
        _prototypePathsToSelections[prototypePath].emplace(selectionKey);
    }

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), prim.primType);
        return true;
    };
    for (const auto& instancerPath : instancerPaths) {
        ForEachPrimInHierarchy(instancerPath, operation);
    }
    for (const auto& prototypePath : prototypePaths) {
        ForEachPrimInHierarchy(prototypePath, operation);
    }
    _SendPrimsAdded(addedPrims);
}

void PiPrototypeWhSi::_DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    // Collect paths
    SelectionKey selectionKey { prototypePath, selectionId };
    if (_selections.find(selectionKey) == _selections.end()) {
        return;
    }
    SelectionData selectionData = _selections.at(selectionKey);

    // Erase from data structures
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    _selections.erase(selectionKey);
    for (const auto& instancerPath : selectionData._instancerPaths) {
        _instancerPathsToSelections[instancerPath].erase(selectionKey);
        if (_instancerPathsToSelections[instancerPath].empty()) {
            _instancerPathsToSelections.erase(instancerPath);
        }
    }
    for (const auto& prototypePath : selectionData._prototypePaths) {
        _prototypePathsToSelections[prototypePath].erase(selectionKey);
        if (_prototypePathsToSelections[prototypePath].empty()) {
            _prototypePathsToSelections.erase(prototypePath);
        }
    }

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
