#include "piPrototypeWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/overlayContainerDataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/pathTable.h>

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _IsPointInstancePrototype(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath) {
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
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

HdSceneIndexBaseRefPtr PiPrototypeWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PiPrototypeWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim PiPrototypeWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = RepathingContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
    return prim;
};

SdfPathVector PiPrototypeWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SdfPathVector childPaths;
    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    auto originalChildPaths = GetInputSceneIndex()->GetChildPrimPaths(originalPath);
    for (const auto& originalChildPath : originalChildPaths) {
        auto itInstancer = _instancerPathsToSelections.upper_bound(originalChildPath);
        bool isInstancerRelevantPath = (itInstancer != _instancerPathsToSelections.end() && itInstancer->first.HasPrefix(originalChildPath)) || (itInstancer != _instancerPathsToSelections.begin() && originalChildPath.HasPrefix((--itInstancer)->first));
        auto itPrototype = _prototypePathsToSelections.upper_bound(originalChildPath);
        bool isPrototypeRelevantPath = (itPrototype != _prototypePathsToSelections.end() && itPrototype->first.HasPrefix(originalChildPath)) || (itPrototype != _prototypePathsToSelections.begin() && originalChildPath.HasPrefix((--itPrototype)->first));
        if (isInstancerRelevantPath || isPrototypeRelevantPath) {
            childPaths.emplace_back(originalChildPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
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
        if (_IsPointInstancePrototype(GetInputSceneIndex(), primPath)) {
            // TODO : Handle path exclusions
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
        if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
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
        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
            }
        }

        // Propagate added prims notifications for prims rooted under a relevant prototype
        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.primType);
                }
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
        auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itSelectedPrim->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        // If an instancer was removed, delete the selection highlights which depended on it
        auto itInstancerParentRemoval = _instancerPathsToSelections.lower_bound(entry.primPath);
        if (itInstancerParentRemoval != _instancerPathsToSelections.end()) {
            if (itInstancerParentRemoval->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itInstancerParentRemoval->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        // If a prototype was removed, delete the selection highlights which depended on it
        auto itPrototypeParentRemoval = _prototypePathsToSelections.lower_bound(entry.primPath);
        if (itPrototypeParentRemoval != _prototypePathsToSelections.end()) {
            if (itPrototypeParentRemoval->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itPrototypeParentRemoval->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        // Propagate removed prims notifications for prims rooted under a relevant instancer
        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
            }
        }

        // Propagate removed prims notifications for prims rooted under a relevant prototype
        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath));
                }
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
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsPointInstancePrototype(GetInputSceneIndex(), entry.primPath)) {
                // Selection changed on the prototype; rebuild the highlight
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    for (const auto& selectionKey : existingSelectionKeys->second) {
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
                for (const auto& selectionKey : instancerSelectionKeysToRebuild->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            auto prototypeSelectionKeysToRebuild = _prototypePathsToSelections.find(entry.primPath);
            if (prototypeSelectionKeysToRebuild != _prototypePathsToSelections.end()) {
                for (const auto& selectionKey : prototypeSelectionKeysToRebuild->second) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    _CreateSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            // No need to dirty in this case since we'll have removed and re-added prims, skip to next entries
            continue;
        }

        // Propagate notifications if this prim is a relevant instancer or a subprim of one
        auto itInstancer = _instancerPathsToSelections.upper_bound(entry.primPath);
        if (itInstancer != _instancerPathsToSelections.begin()) {
            --itInstancer;
            if (entry.primPath.HasPrefix(itInstancer->first)) {
                for (const auto& selectionKey : itInstancer->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }

        // Propagate notifications if this prim is a relevant prototype or a subprim of one
        auto itPrototype = _prototypePathsToSelections.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelections.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionKey : itPrototype->second) {
                    auto selectionPath = SelectionPathFromKey(selectionKey);
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void PiPrototypeWhSi::_CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId)
{
    if (selectionId.empty() || selectionId.find_first_not_of("0123456789") != std::string::npos) {
        // Selection ID is not a positive integer
        return;
    }
    
    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    CollectInstancingPaths(prototypePath, SelectionHighlightsCollectionDirection2::Bidirectional2, instancerPaths, prototypePaths);

    // Setup data structures
    SelectionKey selectionKey { prototypePath, selectionId };
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
