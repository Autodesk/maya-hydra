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

#include "fvpPiInstancerWhSi.h"

#include <flowViewport/tokens.h>

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/instanceIndicesSchema.h>
#include <pxr/imaging/hd/instancedBySchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/selectionSchema.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <set>
#include <string>
#include <utility>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

DEFINE_PRIVATE_OVERRIDEWIREFRAMECOLOR_TOKEN

const std::string kFullHighlight = "Full";
const std::string leadHighlight = "Lead";
const std::string activeHighlight = "Active";

// Computes the mask to use for an instancer's selection highlight
// based on the instancer's topology and the selections.
VtBoolArray
_GetSelectionHighlightMask(const HdInstancerTopologySchema& originalInstancerTopology, const HdSelectionsSchema& selections)
{
    VtBoolArray originalMask = 
        originalInstancerTopology.GetMask()->GetTypedValue(0);

    size_t nbInstances = 0;
    auto instanceIndices = originalInstancerTopology.GetInstanceIndices();

    for (size_t iInstanceIndex = 0; iInstanceIndex < instanceIndices.GetNumElements(); iInstanceIndex++) {
        auto protoInstances = instanceIndices.GetElement(iInstanceIndex)->GetTypedValue(0);
        nbInstances += protoInstances.size();
    }
    if (!TF_VERIFY(originalMask.empty() || originalMask.size() == nbInstances, "Instancer mask has incorrect size.")) {
        return originalMask;
    }

    // Initialize return mask
    VtBoolArray selectionHighlightMask = 
        selections.IsDefined() ? VtBoolArray(nbInstances, false) : 
        (originalMask.empty() ? VtBoolArray(nbInstances, true) : originalMask);

    // Loop over all selections.
    const auto nbSelections = selections.GetNumElements();
    for (size_t snNdx = 0; snNdx < nbSelections; ++snNdx) {
        const auto sn = selections.GetElement(snNdx);
        // Instancer is expected to be marked "fully selected" even if only
        // certain instances are selected, based on USD's _AddToSelection
        // function in selectionSceneIndexObserver.cpp :
        // https://github.com/PixarAnimationStudios/OpenUSD/blob/f7b8a021ce3d13f91a0211acf8a64a8b780524df/pxr/imaging/hdx/selectionSceneIndexObserver.cpp#L212-L251
        const auto fullySelected = sn.GetFullySelected();
        if (!fullySelected || !fullySelected->GetTypedValue(0)) {
            return originalMask;
        }

        const auto nestedInstanceIndices = sn.GetNestedInstanceIndices();
        if (!nestedInstanceIndices) {
            // We have a selection that has no instances, which means the whole
            // instancer is selected : this overrides any instances selection.
            return originalMask;
        }

        for (size_t iInstanceIndices = 0; iInstanceIndices < nestedInstanceIndices.GetNumElements(); iInstanceIndices++) {
            auto instanceIndices = nestedInstanceIndices.GetElement(0);
            for (const auto& instanceIndex : instanceIndices.GetInstanceIndices()->GetTypedValue(0)) {
                selectionHighlightMask[instanceIndex] = originalMask.empty() ? true : originalMask[instanceIndex];
            }
        }
    }
    return selectionHighlightMask;
}

// Returns the overall data source for an instancer's selection highlight.
// This replaces the mask data source.
HdContainerDataSourceHandle
_GetSelectionHighlightInstancerDataSource(const HdContainerDataSourceHandle& originalDataSource, const HdSelectionsSchema& selections)
{
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(originalDataSource);

    HdContainerDataSourceEditor editedDataSource = HdContainerDataSourceEditor(originalDataSource);

    if (selections.IsDefined()) {
        HdDataSourceLocator maskLocator = HdInstancerTopologySchema::GetDefaultLocator().Append(HdInstancerTopologySchemaTokens->mask);
        VtBoolArray selectionHighlightMask = _GetSelectionHighlightMask(instancerTopology, selections);
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

bool PiInstancerWhSi::_ConditionallyCreateSelectionHighlight(
    const PXR_NS::HdSceneIndexPrim& instancerPrim,
    const PXR_NS::SdfPath&          instancerPath
)
{
    auto selectionsSchema = HdSelectionsSchema::GetFromParent(instancerPrim.dataSource);

    if (HasFullySelectedAncestorInclusive(instancerPath)) {
        _CreateSelectionHighlight(instancerPrim, instancerPath, selectionsSchema, kFullHighlight);
        return true;
    }
    else if (selectionsSchema.IsDefined()) {
        // Calling the HdSelectionsSchema overload is the O(1) equivalent
        // of the following:
        //
        // for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
        //     _CreateSelectionHighlight(instancerPrim, instancerPath, selectionsSchema, std::to_string(selectionId));
        // }

        _CreateSelectionHighlight(instancerPrim, instancerPath, selectionsSchema);
        return true;
    }
    return false;
}

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
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, 
            _wireframeColorInterface->getWireframeColor(selectionPath));
    }
    else if (_IsPointInstancer(prim) && originalPath == selectionKey.first && selectionKey.second != kFullHighlight) {
        // Adjust the instancer mask to only show selected instances
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        prim.dataSource = _GetSelectionHighlightInstancerDataSource(prim.dataSource, selectionsSchema);
    }
    prim.dataSource = RepathInstancingDataSources(prim.dataSource, SdfPath::AbsoluteRootPath(), selectionPath);
    return prim;
};

SdfPathVector PiInstancerWhSi::GetHighlightChildPrimPaths(const SdfPath &selectionPath, const SdfPath &fullPrimPath) const
{
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
            _ConditionallyCreateSelectionHighlight(prim, primPath);
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
            if (_ConditionallyCreateSelectionHighlight(prim, entry.primPath)) {
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

void PiInstancerWhSi::ProcessRemovedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        // Delete all selection highlights for point instancers rooted under the removed prim
        auto itPointInstancer = FindSelfOrFirstChild(entry.primPath, _pointInstancerPaths);
        while (itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(entry.primPath)) {
            itPointInstancer = _pointInstancerPaths.erase(itPointInstancer);
        }

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

void PiInstancerWhSi::ProcessDirtiedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator()) ||
            entry.dirtyLocators.Intersects(primvarsOverrideWireframeColorLocator)) {
            HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
            if (_IsPointInstancer(prim)) {
                // Selection changed on the instancer or lead object status changed; rebuild the highlights
                auto existingSelectionKeys = _primPathsToSelections.find(entry.primPath);
                if (existingSelectionKeys != _primPathsToSelections.end()) {
                    const auto selectionKeysToDelete = existingSelectionKeys->second;
                    for (const auto& selectionKey : selectionKeysToDelete) {
                        _DeleteSelectionHighlight(selectionKey.first, selectionKey.second);
                    }
                }

                if (_ConditionallyCreateSelectionHighlight(prim, entry.primPath)) {
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
                highlightEntries.emplace_back(entry.primPath.ReplacePrefix(SdfPath::AbsoluteRootPath(), selectionPath), entry.dirtyLocators);
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void PiInstancerWhSi::ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected)
{
    if (isFullySelected) {
        for (auto itPointInstancer = FindSelfOrFirstChild(primPath, _pointInstancerPaths); itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(primPath); itPointInstancer++) {
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
        for (auto itPointInstancer = FindSelfOrFirstChild(primPath, _pointInstancerPaths); itPointInstancer != _pointInstancerPaths.end() && itPointInstancer->HasPrefix(primPath); itPointInstancer++) {
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

void PiInstancerWhSi::_CreateSelectionHighlight(const SdfPath& instancerPath, const std::string& selectionId)
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(instancerPath);
    _CreateSelectionHighlight(
        prim, instancerPath, HdSelectionsSchema::GetFromParent(prim.dataSource),
        selectionId);
}

void PiInstancerWhSi::_CreateSelectionHighlight(
    const HdSceneIndexPrim&   instancerPrim,
    const SdfPath&            instancerPath,
    const HdSelectionsSchema& selectionsSchema
)
{
    // Separate the selections into lead and active based on actual selection status
    auto [leadSelections, activeSelections] = _SeparateLeadAndActiveSchemas(selectionsSchema, instancerPath);
    
    // Create separate hierarchies with the filtered selections
    if (leadSelections.IsDefined()) {
        _CreateSelectionHighlight(instancerPrim, instancerPath, leadSelections, leadHighlight);
    }
    
    if (activeSelections.IsDefined()) {
        _CreateSelectionHighlight(instancerPrim, instancerPath, activeSelections, activeHighlight);
    }
}

void PiInstancerWhSi::_CreateSelectionHighlight(
    const PXR_NS::HdSceneIndexPrim&   instancerPrim,
    const PXR_NS::SdfPath&            instancerPath,
    const PXR_NS::HdSelectionsSchema& selectionsSchema,
    const std::string&                selectionId
)
{
    SelectionKey selectionKey { instancerPath, selectionId };
    if (_selections.find(selectionKey) != _selections.end()) {
        return;
    }

    // Collect paths
    SdfPathSet instancerPaths;
    SdfPathSet prototypePaths;
    CollectInstancingPaths(instancerPath, InstancingPathsCollectionDirection::Bidirectional, instancerPaths, prototypePaths);

    // Setup data structures
    SdfPath selectionPath = RegisterSelection(selectionKey);

    SelectionData selectionData;
    if (selectionId == kFullHighlight) {
        selectionData._primSelection = PrimSelection {instancerPath};
    } else if (selectionId == leadHighlight || selectionId == activeHighlight) {
        selectionData._primSelection = PrimSelection {instancerPath};
    } else {
        selectionData._primSelection = ConvertHydraToFvpSelection(instancerPath, selectionsSchema.GetElement(std::stoul(selectionId)));
    }
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
    // For dual hierarchy, delete both lead and active hierarchies when the main "0" selection is removed
    if (selectionId == "0") {
        _DeleteSelectionHighlight(instancerPath, leadHighlight);
        _DeleteSelectionHighlight(instancerPath, activeHighlight);
        return;
    }

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

std::pair<PXR_NS::HdSelectionsSchema, PXR_NS::HdSelectionsSchema> PiInstancerWhSi::_SeparateLeadAndActiveSchemas(
    const HdSelectionsSchema& originalSelections,
    const SdfPath& instancerPath
) {
    std::vector<HdDataSourceBaseHandle> leadSelectionList;
    std::vector<HdDataSourceBaseHandle> activeSelectionList;
    
    // Get the instancer topology to access prototype paths
    HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
    HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);
    
    for (size_t selectionId = 0; selectionId < originalSelections.GetNumElements(); selectionId++) {
        auto selection = originalSelections.GetElement(selectionId);
        auto primSelection = ConvertHydraToFvpSelection(instancerPath, selection);
        bool isLead = false;
        
        if (_wireframeColorInterface) {
            // Check if the instancer itself is the lead object
            isLead = _wireframeColorInterface->isLeadObject(primSelection.primPath);
            
            // If not, check if any of the prototypes are lead objects
            if (!isLead && instancerTopology.IsDefined()) {
                auto protoPaths = instancerTopology.GetPrototypes()->GetTypedValue(0);
                for (const auto& protoPath : protoPaths) {
                    if (_wireframeColorInterface->isLeadObject(protoPath)) {
                        isLead = true;
                        break;
                    }
                }
            }
        }
        
        if (isLead) {
            leadSelectionList.push_back(selection.GetContainer());
        } else {
            activeSelectionList.push_back(selection.GetContainer());
        }
    }
    
    HdSelectionsSchema leadSelections = !leadSelectionList.empty() ?
        HdSelectionsSchema(HdRetainedSmallVectorDataSource::New(leadSelectionList.size(), leadSelectionList.data())) :
        HdSelectionsSchema(nullptr);
    
    HdSelectionsSchema activeSelections = !activeSelectionList.empty() ?
        HdSelectionsSchema(HdRetainedSmallVectorDataSource::New(activeSelectionList.size(), activeSelectionList.data())) :
        HdSelectionsSchema(nullptr);
    
    return std::make_pair(leadSelections, activeSelections);
}

}
