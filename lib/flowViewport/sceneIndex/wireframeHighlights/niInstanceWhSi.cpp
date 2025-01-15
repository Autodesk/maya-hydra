#include "niInstanceWhSi.h"
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

HdSceneIndexBaseRefPtr NiInstanceWhSi::New(
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
    prim.dataSource = RepathingContainerDataSource::New(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath, prim.dataSource);
    HdContainerDataSourceEditor dsEditor(prim.dataSource);
    dsEditor.Set(HdInstancedBySchema::GetDefaultLocator(), HdBlockDataSource::New());

    HdSceneIndexPrim instancePrim = GetInputSceneIndex()->GetPrim(selectionKey.first);
    auto instanceXform = HdXformSchema::GetFromParent(instancePrim.dataSource).GetMatrix()->GetTypedValue(0);
    auto prototypeXform = HdXformSchema::GetFromParent(prim.dataSource).GetMatrix()->GetTypedValue(0);
    dsEditor.Set(HdXformSchema::GetDefaultLocator().Append(HdXformSchemaTokens->matrix), HdRetainedTypedSampledDataSource<GfMatrix4d>::New(instanceXform * prototypeXform));

    prim.dataSource = dsEditor.Finish();
    if (prim.primType == HdPrimTypeTokens->mesh) {
        prim.dataSource = SetWireframeRepr(prim.dataSource, _wireframeColorInterface->getWireframeColor(selectionKey.first));
    }
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
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(primPath);
                }
            }
        }
        HdInstanceSchema instance = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instance.IsDefined()) {
            _instancePaths.emplace(primPath);
        }
        return true;
    };
    ForEachPrimInHierarchy(SdfPath::AbsoluteRootPath(), operation);

    for (const auto& instancePath : _instancePaths) {
        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(instancePath);
        if (itSelectedParentPath != _fullySelectedPaths.begin() && instancePath.HasPrefix(*std::prev(itSelectedParentPath))) {
            _CreateSelectionHighlight(instancePath);
        }
    }
}

void NiInstanceWhSi::ProcessAddedPrims(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries highlightEntries;
    for (const auto& entry : entries) {
        bool isInstance = false;
        bool isSelected = false;
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        HdInstanceSchema instance = HdInstanceSchema::GetFromParent(prim.dataSource);
        if (instance.IsDefined()) {
            _instancePaths.emplace(entry.primPath);
            isInstance = true;
        }
        HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
        if (selectionsSchema.IsDefined()) {
            for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                    _fullySelectedPaths.emplace(entry.primPath);
                    isSelected = true;
                }
            }
        }
        if (isInstance) {
            auto itSelectedParent = _fullySelectedPaths.upper_bound(entry.primPath);
            if (itSelectedParent != _fullySelectedPaths.begin() && entry.primPath.HasPrefix(*std::prev(itSelectedParent))) {
                if (_highlightedInstancePaths.find(entry.primPath) == _highlightedInstancePaths.end()) {
                    _CreateSelectionHighlight(entry.primPath);
                }
            }
        }
        if (isSelected) {
            auto itInstanceChild = _instancePaths.lower_bound(entry.primPath);
            while (itInstanceChild != _instancePaths.end() && itInstanceChild->HasPrefix(entry.primPath)) {
                if (_highlightedInstancePaths.find(*itInstanceChild) == _highlightedInstancePaths.end()) {
                    _CreateSelectionHighlight(*itInstanceChild);
                }
                itInstanceChild++;
            }
        }
        auto itPrototype = _prototypePathsToSelectionPaths.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelectionPaths.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionPath : itPrototype->second) {
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath), entry.primType);
                }
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
        auto itInstance = _instancePaths.lower_bound(entry.primPath);
        while (itInstance != _instancePaths.end() && itInstance->HasPrefix(entry.primPath)) {
            if (_highlightedInstancePaths.find(*itInstance) != _highlightedInstancePaths.end()) {
                _DeleteSelectionHighlight(*itInstance);
            }
            itInstance = _instancePaths.erase(itInstance);
        }

        auto itSelectedPath = _fullySelectedPaths.lower_bound(entry.primPath);
        while (itSelectedPath != _fullySelectedPaths.end() && itSelectedPath->HasPrefix(entry.primPath)) {
            itSelectedPath = _fullySelectedPaths.erase(itSelectedPath);
            // Child instance highlights will have already been deleted above, since deleting a selected parent
            // implies deleting child instances.
        }

        auto itSelectedPrim = _primPathsToSelections.lower_bound(entry.primPath);
        if (itSelectedPrim != _primPathsToSelections.end()) {
            if (itSelectedPrim->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionKey : itSelectedPrim->second) {
                    _DeleteSelectionHighlight(selectionKey.first);
                }
            }
        }

        auto itPrototypeParentRemoval = _prototypePathsToSelectionPaths.lower_bound(entry.primPath);
        if (itPrototypeParentRemoval != _prototypePathsToSelectionPaths.end()) {
            if (itPrototypeParentRemoval->first.HasPrefix(entry.primPath)) {
                for (const auto& selectionPath : itPrototypeParentRemoval->second) {
                    _DeleteSelectionHighlight(SelectionKeyFromPath(selectionPath).first);
                }
            }
        }

        auto itPrototype = _prototypePathsToSelectionPaths.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelectionPaths.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionPath : itPrototype->second) {
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath));
                }
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
        HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (entry.dirtyLocators.Intersects(HdSelectionsSchema::GetDefaultLocator())) {
            bool isFullySelected = false;
            HdSelectionsSchema selectionsSchema = HdSelectionsSchema::GetFromParent(prim.dataSource);
            if (selectionsSchema.IsDefined()) {
                for (size_t selectionId = 0; selectionId < selectionsSchema.GetNumElements(); selectionId++) {
                    if (selectionsSchema.GetElement(selectionId).GetFullySelected()) {
                        isFullySelected = true;
                    }
                }
            }
            // Newly selected path : create selection highlights for all instances not yet highlighted under it.
            if (isFullySelected && _fullySelectedPaths.find(entry.primPath) == _fullySelectedPaths.end()) {
                _fullySelectedPaths.emplace(entry.primPath);
                auto itInstance = _instancePaths.lower_bound(entry.primPath);
                while (itInstance != _instancePaths.end() && itInstance->HasPrefix(entry.primPath)) {
                    if (_highlightedInstancePaths.find(*itInstance) == _highlightedInstancePaths.end()) {
                        _CreateSelectionHighlight(*itInstance);
                    }
                    itInstance++;
                }
            }
            // Newly unselected path : delete selection highlights for all instances under it if no other prim
            // that is a parent of that instance is selected.
            else if (!isFullySelected && _fullySelectedPaths.find(entry.primPath) != _fullySelectedPaths.end()) {
                _fullySelectedPaths.erase(entry.primPath);
                auto itInstance = _instancePaths.lower_bound(entry.primPath);
                while (itInstance != _instancePaths.end() && itInstance->HasPrefix(entry.primPath)) {
                    if (_highlightedInstancePaths.find(*itInstance) != _highlightedInstancePaths.end()) {
                        auto itSelectedParentPath = _fullySelectedPaths.upper_bound(*itInstance);
                        if (itSelectedParentPath == _fullySelectedPaths.begin() || !itInstance->HasPrefix(*std::prev(itSelectedParentPath))) {
                            // No selected parent.
                            _DeleteSelectionHighlight(*itInstance);
                        }
                    }
                    itInstance++;
                }
            }
        }

        if (_highlightedInstancePaths.find(entry.primPath) != _highlightedInstancePaths.end()) {
            // If instance structure was dirtied, rebuild the highlight
            if (entry.dirtyLocators.Intersects(HdInstanceSchema::GetDefaultLocator())) {
                _DeleteSelectionHighlight(entry.primPath);
                _CreateSelectionHighlight(entry.primPath);
            }
            // Dirty prototype prims (to update highlight color)
            auto selectionPath = SelectionPathFromKey(SelectionKey(entry.primPath, ""));
            auto dirtyOperation = [&](const SdfPath& primPath, const HdSceneIndexPrim& prim) -> bool {
                highlightEntries.emplace_back(primPath.ReplacePrefix(_selectionPathsToPrototypePrefixes.at(selectionPath), selectionPath), entry.dirtyLocators);
                return true;
            };
            auto prototypePath = _GetNativeInstancePrototypePath(GetInputSceneIndex(), entry.primPath);
            ForEachPrimInHierarchy(prototypePath, dirtyOperation);
        }
        
        // Propagate notifications to the prototype highlight if the dirtied prim is a relevant prototype or a subprim of one
        auto itPrototype = _prototypePathsToSelectionPaths.upper_bound(entry.primPath);
        if (itPrototype != _prototypePathsToSelectionPaths.begin()) {
            --itPrototype;
            if (entry.primPath.HasPrefix(itPrototype->first)) {
                for (const auto& selectionPath : itPrototype->second) {
                    highlightEntries.emplace_back(entry.primPath.ReplacePrefix(itPrototype->first.GetParentPath(), selectionPath), entry.dirtyLocators);
                }
            }
        }
    }
    _SendPrimsDirtied(highlightEntries);
}

void NiInstanceWhSi::_CreateSelectionHighlight(const SdfPath& instancePath)
{
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
    _highlightedInstancePaths.emplace(instancePath);

    // Send notifications
    HdSceneIndexObserver::AddedPrimEntries addedPrims;
    auto operation = [&addedPrims, prototypePath, selectionPath](const pxr::SdfPath& primPath, const pxr::HdSceneIndexPrim& prim) -> bool {
        addedPrims.emplace_back(primPath.ReplacePrefix(prototypePath.GetParentPath(), selectionPath), prim.primType);
        return true;
    };
    ForEachPrimInHierarchy(prototypePath, operation);

    // Send notifications
    _SendPrimsAdded(addedPrims);
}

void NiInstanceWhSi::_DeleteSelectionHighlight(const SdfPath& instancePath)
{
    // Erase from data structures
    SelectionKey selectionKey { instancePath, "" };
    SdfPath selectionPath = UnregisterSelection(selectionKey);
    auto prototypePrefix = _selectionPathsToPrototypePrefixes.at(selectionPath);
    auto itPrototypePath = _prototypePathsToSelectionPaths.upper_bound(prototypePrefix);
    if (itPrototypePath != _prototypePathsToSelectionPaths.end()) {
        itPrototypePath->second.erase(selectionPath);
    }
    _selectionPathsToPrototypePrefixes.erase(selectionPath);
    _highlightedInstancePaths.erase(instancePath);

    // Send notifications
    _SendPrimsRemoved({selectionPath});
}

}
