#include "piInstancerWhSi.h"
#include <flowViewport/fvpUtils.h>
#include "baseWhSi.h"
#include "baseWireframeHighlightSi.h"
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
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

const std::string kFullHighlight = "Full";

// Computes the mask to use for an instancer's selection highlight
// based on the instancer's topology and the selection.
VtBoolArray
_GetSelectionHighlightMask(const HdInstancerTopologySchema& originalInstancerTopology, const HdSelectionSchema& selection)
{
    // Schema getters were made const in USD 24.05 (specifically Hydra API version 66).
    // We work around this for previous versions by const casting.
    VtBoolArray originalMask = 
#if HD_API_VERSION < 66
    const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetMask()->GetTypedValue(0);
#else
    originalInstancerTopology.GetMask()->GetTypedValue(0);
#endif

    size_t nbInstances = 0;
    auto instanceIndices = 
#if HD_API_VERSION < 66
    const_cast<HdInstancerTopologySchema&>(originalInstancerTopology).GetInstanceIndices();
#else
    originalInstancerTopology.GetInstanceIndices();
#endif
    for (size_t iInstanceIndex = 0; iInstanceIndex < instanceIndices.GetNumElements(); iInstanceIndex++) {
        auto protoInstances = instanceIndices.GetElement(iInstanceIndex)->GetTypedValue(0);
        nbInstances += protoInstances.size();
    }
    if (!TF_VERIFY(originalMask.empty() || originalMask.size() == nbInstances, "Instancer mask has incorrect size.")) {
        return originalMask;
    }
    VtBoolArray selectionHighlightMask = [&]() {
        if (!selection.IsDefined()) {
            return originalMask.empty() ? VtBoolArray(nbInstances, true) : originalMask;
        }
        return VtBoolArray(nbInstances, false);
    }();

    // Instancer is expected to be marked "fully selected" even if only certain instances are selected,
    // based on USD's _AddToSelection function in selectionSceneIndexObserver.cpp :
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/imaging/hdx/selectionSceneIndexObserver.cpp#L212-L251
    if (!selection.GetFullySelected() || !selection.GetFullySelected()->GetTypedValue(0)) {
        return originalMask;
    }
    if (!selection.GetNestedInstanceIndices()) {
        // We have a selection that has no instances, which means the whole instancer is selected :
        // this overrides any instances selection.
        return originalMask;
    }
    HdInstanceIndicesVectorSchema nestedInstanceIndices = selection.GetNestedInstanceIndices();
    for (size_t iInstanceIndices = 0; iInstanceIndices < nestedInstanceIndices.GetNumElements(); iInstanceIndices++) {
        HdInstanceIndicesSchema instanceIndices = nestedInstanceIndices.GetElement(0);
        for (const auto& instanceIndex : instanceIndices.GetInstanceIndices()->GetTypedValue(0)) {
            selectionHighlightMask[instanceIndex] = originalMask.empty() ? true : originalMask[instanceIndex];
        }
    }
    return selectionHighlightMask;
}

// Returns the overall data source for an instancer's selection highlight.
// This replaces the mask data source.
HdContainerDataSourceHandle
_GetSelectionHighlightInstancerDataSource(const HdContainerDataSourceHandle& originalDataSource, const HdSelectionSchema& selection)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(originalDataSource);

    HdContainerDataSourceEditor editedDataSource = HdContainerDataSourceEditor(originalDataSource);

    if (selection.IsDefined()) {
        HdDataSourceLocator maskLocator = HdInstancerTopologySchema::GetDefaultLocator().Append(HdInstancerTopologySchemaTokens->mask);
        VtBoolArray selectionHighlightMask = _GetSelectionHighlightMask(instancerTopology, selection);
        auto selectionHighlightMaskDataSource = HdRetainedTypedSampledDataSource<VtBoolArray>::New(selectionHighlightMask);
        editedDataSource.Set(maskLocator, selectionHighlightMaskDataSource);
    }

    return editedDataSource.Finish();
}

bool _IsPointInstancer(const HdSceneIndexPrim& prim) {
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(prim.dataSource);
    HdInstancedBySchema instancedBy = HdInstancedBySchema::GetFromParent(prim.dataSource);
    return prim.primType == HdPrimTypeTokens->instancer && instancerTopology.IsDefined() && !instancerTopology.GetInstanceLocations() && !instancedBy.IsDefined();
}

}

namespace FVP_NS_DEF {

PiInstancerWhSiRefPtr PiInstancerWhSi::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface)
{
    return TfCreateRefPtr(new PiInstancerWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface));
}

HdSceneIndexPrim PiInstancerWhSi::GetHighlightPrim(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
    SelectionKey selectionKey = SelectionKeyFromPath(selectionPath);
    auto primSelection = _selections.at(selectionKey)._primSelection;

    auto originalPath = fullPrimPath.ReplacePrefix(selectionPath, SdfPath::AbsoluteRootPath());
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(originalPath);
    prim.dataSource = RepathingContainerDataSource::New(SdfPath::AbsoluteRootPath(), selectionPath, prim.dataSource);
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, primSelection.nestedInstanceIndices.empty() ? _wireframeColorInterface->getWireframeColor(selectionKey.first) : _wireframeColorInterface->getWireframeColor(primSelection));
    }
    if (_IsPointInstancer(prim) && originalPath == selectionKey.first && selectionKey.second != kFullHighlight) {
        // Adjust the instancer mask to only show selected instances
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        HdSelectionSchema activeSelection = selectionsSchema.GetElement(std::stoul(selectionKey.second));
        prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource, activeSelection);
    }
    return prim;
};

SdfPathVector PiInstancerWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
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

PiInstancerWhSi::PiInstancerWhSi(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const SdfPath& highlightHierarchyPrefix,
    const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
) : BaseWhSi(inputSceneIndex, highlightHierarchyPrefix, wireframeColorInterface)
{
    auto operation = [this](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
        if (IsExcludedPath(primPath)) {
            return false;
        }
        if (_IsPointInstancer(prim)) {
            _pointInstancerPaths.emplace(primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (HasFullySelectedAncestorInclusive(primPath)) {
                _CreateSelectionHighlight(primPath, kFullHighlight);
            }
            else if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    _CreateSelectionHighlight(primPath, std::to_string(selectionId));
                }
            }
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);
}

void PiInstancerWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (_IsPointInstancer(prim)) {
            _pointInstancerPaths.emplace(entry.primPath);
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (HasFullySelectedAncestorInclusive(entry.primPath)) {
                _CreateSelectionHighlight(entry.primPath, kFullHighlight);
                // We just created the highlight, no need to add highlight prims
                continue;
            }
            else if (selectionsSchema.IsDefined()) {
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

void PiInstancerWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        auto itPointInstancer = _pointInstancerPaths.lower_bound(entry.primPath);
        while (itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(entry.primPath)) {
            itPointInstancer = _pointInstancerPaths.erase(itPointInstancer);
        }

        // If a parent of one the selected prims was removed, delete the selection highlight
        auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
                const auto selectionKeysToDelete = itSelectedPrim->second;
                for (const auto& selectionKey : selectionKeysToDelete) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        // If an instancer was removed, delete the selection highlights which depended on it
        auto itInstancerParentRemoval = _instancerPathsToSelections.lower_bound(entry.primPath);
        if (itInstancerParentRemoval != _instancerPathsToSelections.end()) {
            if (itInstancerParentRemoval->first.HasPrefix(entry.primPath)) {
                const auto selectionKeysToDelete = itInstancerParentRemoval->second;
                for (const auto& selectionKey : selectionKeysToDelete) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
        }

        // If a prototype was removed, delete the selection highlights which depended on it
        auto itPrototypeParentRemoval = _prototypePathsToSelections.lower_bound(entry.primPath);
        if (itPrototypeParentRemoval != _prototypePathsToSelections.end()) {
            if (itPrototypeParentRemoval->first.HasPrefix(entry.primPath)) {
                const auto selectionKeysToDelete = itPrototypeParentRemoval->second;
                for (const auto& selectionKey : selectionKeysToDelete) {
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

void PiInstancerWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsPointInstancer(prim)) {
                // Selection changed on the instancer; rebuild the highlight
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    const auto selectionKeysToDelete = existingSelectionKeys->second;
                    for (const auto& selectionKey : selectionKeysToDelete) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
                if (HasFullySelectedAncestorInclusive(entry.primPath)) {
                    _CreateSelectionHighlight(entry.primPath, kFullHighlight);
                    // We rebuilt the highlight, no need to do the rest
                    continue;
                }
                else if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        _CreateSelectionHighlight(entry.primPath, std::to_string(selectionId));
                    }
                    // We rebuilt the highlight, no need to do the rest
                    continue;
                }
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

void PiInstancerWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    if (isFullySelected) {
        for (auto itPointInstancer = _pointInstancerPaths.lower_bound(primPath); itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(primPath); itPointInstancer++) {
            auto existingSelectionKeys = _primPathsToSelections.find(*itPointInstancer);
            if (existingSelectionKeys != _primPathsToSelections.end()) {
                const auto selectionKeysToDelete = existingSelectionKeys->second;
                for (const auto& selectionKey : selectionKeysToDelete) {
                    _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                }
            }
            _CreateSelectionHighlight(*itPointInstancer, kFullHighlight);
        }
    }
    else {
        for (auto itPointInstancer = _pointInstancerPaths.lower_bound(primPath); itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(primPath); itPointInstancer++) {
            if (!HasFullySelectedAncestorInclusive(*itPointInstancer)) {
                _DeleteSelectionHighlight(*itPointInstancer, kFullHighlight);
                HdSceneIndexPrim pointInstancerPrim = GetInputSceneIndex()->GetPrim(*itPointInstancer);
                HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(pointInstancerPrim.dataSource);
                if (selectionsSchema.IsDefined()) {
                    for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                        _CreateSelectionHighlight(*itPointInstancer, std::to_string(selectionId));
                    }
                }
            }
        }
    }
}

void PiInstancerWhSi::_CreateSelectionHighlight(const SdfPath& instancerPath, std::string selectionId)
{
    SelectionKey selectionKey { instancerPath, selectionId };
    if (_selections.find(selectionKey) != _selections.end()) {
        return;
    }

    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    CollectInstancingPaths(instancerPath, SelectionHighlightsCollectionDirection2::Bidirectional2, instancerPaths, prototypePaths);

    // Setup data structures
    SdfPath selectionPath = RegisterSelection(selectionKey);

    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(instancerPath);
    HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
    SelectionData selectionData;
    selectionData._primSelection = selectionId == kFullHighlight ? PrimSelection {instancerPath} : ConvertHydraToFvpSelection(instancerPath, selectionsSchema.GetElement(std::stoul(selectionId)));
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

void PiInstancerWhSi::_DeleteSelectionHighlight(const SdfPath& instancerPath, std::string selectionId)
{
    // Collect paths
    SelectionKey selectionKey { instancerPath, selectionId };
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
