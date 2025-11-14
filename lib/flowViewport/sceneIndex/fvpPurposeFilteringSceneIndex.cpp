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

#include "fvpPurposeFilteringSceneIndex.h"
#include "flowViewport/fvpUtils.h"

#include <pxr/imaging/hd/purposeSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
// Set of known purposes used for filtering.
Fvp::PurposeFilteringSceneIndex::Purposes filteringPurposes({
        TfToken("render"), TfToken("guide"), TfToken("proxy")});
}

namespace FVP_NS_DEF {
    
PurposeFilteringSceneIndexRefPtr PurposeFilteringSceneIndex::New(
    const HdSceneIndexBaseRefPtr&    inputSceneIndex,
    const Purposes& includedPurposes)
{
    // If we return TfCreateRefPtr directly, Clang will destroy the rvalue before
    // returning, which means we will return a null pointer. To avoid this, store 
    // the pointer in an lvalue first and return that.
    auto refPtr = TfCreateRefPtr(new PurposeFilteringSceneIndex(inputSceneIndex, includedPurposes));
    return refPtr;
}

PurposeFilteringSceneIndex::PurposeFilteringSceneIndex(
    HdSceneIndexBaseRefPtr const&    inputSceneIndex,
    const Purposes& includedPurposes)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _includedPurposes(includedPurposes)
{
    TF_AXIOM(inputSceneIndex);
    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        _UpdateFilteringStatus(primPath);
    }
}

bool PurposeFilteringSceneIndex::_IsAncestorFilteredOutInclusive(const SdfPath& primPath) const
{
    return FindSelfOrFirstParent(primPath, _filteredPrims) != _filteredPrims.end();
}

bool PurposeFilteringSceneIndex::_FilterOut(const SdfPath& primPath) const
{
    if (primPath.IsAbsoluteRootPath()) {
        return false; // Always include root prims
    }

    // Now do the expensive GetPrim() call only when necessary
    const auto& inputSceneIndex = GetInputSceneIndex();
    auto prim = inputSceneIndex->GetPrim(primPath);

    // Algorithm:
    // - Prim with no purpose: pass it through (unfiltered)
    // - Prim with unknown purpose: pass it through (unfiltered).
    //   Known purposes are is {render, proxy, guide}
    // - Prim with known purpose: pass it through only if in set of
    //   included purposes.

    // No data source: unfiltered.
    if (!prim.dataSource) {
        return false;
    }

    // No purpose: unfiltered.
    const TfToken purpose = GetPurposeRenderTag(prim.dataSource);
    if (purpose.IsEmpty()) {
        return false;
    }

    // Not a filtering purpose: unfiltered.
    if (filteringPurposes.find(purpose) == filteringPurposes.end()) {
        return false;
    }

    // Not included: FILTERED OUT.
    if (_includedPurposes.find(purpose) == _includedPurposes.end()) {
        return true;
    }

    return false; // Default: will be rendered.
}

void PurposeFilteringSceneIndex::_UpdateFilteringStatus(const PXR_NS::SdfPath& primPath)
{
    if (_IsAncestorFilteredOutInclusive(primPath) && _filteredPrims.find(primPath) == _filteredPrims.end()) {
        // This is a child of a filtered prim. Nothing to do.
        return;
    }

    if (_FilterOut(primPath)) {
        // Remove child paths so only root filtered prims are kept. When one is unfiltered, we recheck its children anyways
        for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
            if ((*it).HasPrefix(primPath)) {
                it = _filteredPrims.erase(it);
            } else {
                ++it;
            }
        }
        _filteredPrims.insert(primPath);
    } else {
        _filteredPrims.erase(primPath);
    }
}

void PurposeFilteringSceneIndex::_UpdateFilteringForTree(const PXR_NS::SdfPath& primPath)
{
    const bool wasFiltered = _IsAncestorFilteredOutInclusive(primPath);
    if (wasFiltered && _filteredPrims.find(primPath) == _filteredPrims.end()) {
        // This is a child of a filtered prim. Nothing to do.
        return;
    }
    _UpdateFilteringStatus(primPath);
    const bool isFiltered = _IsAncestorFilteredOutInclusive(primPath);
    if (wasFiltered == isFiltered) {
        // No change in filtering status: nothing to do
        return;
    }

    // The filtering status changed.
    if (isFiltered) {
        _SendPrimsRemoved({primPath});
    } else {
        // Prim was just unfiltered. Need to check its children.
        // Update the whole sub-hierarchy
        HdSceneIndexObserver::AddedPrimEntries addedEntries;
        HdSceneIndexPrimView subTree(GetInputSceneIndex(), primPath);
        for (auto itTree = subTree.begin(); itTree != subTree.end(); ++itTree) {
            const SdfPath& currPath = *itTree;
            // HdSceneIndexPrimView includes the prim itself
            if (currPath != primPath) {
                _UpdateFilteringStatus(currPath);
            }
            if (_filteredPrims.find(currPath) == _filteredPrims.end()) {
                addedEntries.emplace_back(currPath, GetInputSceneIndex()->GetPrim(currPath).primType);
            } else {
                itTree.SkipDescendants();
            }
        }
        _SendPrimsAdded(addedEntries);
    }
}

HdSceneIndexPrim PurposeFilteringSceneIndex::GetPrim(const SdfPath& primPath) const
{
    // If an ancestor if filtered out, return nothing
    if (_IsAncestorFilteredOutInclusive(primPath)) {
        return {};
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PurposeFilteringSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    // If this prim is filtered out, return no children
    if (_IsAncestorFilteredOutInclusive(primPath)) {
        return {};
    }

    // Get children from input scene index
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);

    // Filter out children that should be filtered out
    SdfPathVector filteredChildPaths;
    for (const SdfPath& childPath : childPaths) {
        if (!_IsAncestorFilteredOutInclusive(childPath)) {
            filteredChildPaths.push_back(childPath);
        }
    }
    return filteredChildPaths;
}

void PurposeFilteringSceneIndex::UpdatePrimsFromIncludedPurposes(
    const Purposes& includedPurposes
)
{
    _includedPurposes = includedPurposes;
    auto& inputSceneIndex = GetInputSceneIndex();
    for (const SdfPath& primPath : HdSceneIndexPrimView(inputSceneIndex)) {
        _UpdateFilteringForTree(primPath);
    }
}

void PurposeFilteringSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries addedEntries;

    for (const auto& addedEntry : entries) {
        _UpdateFilteringForTree(addedEntry.primPath);
        if (!_IsAncestorFilteredOutInclusive(addedEntry.primPath)) {
            addedEntries.emplace_back(addedEntry);
        }
    }

    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
}

void PurposeFilteringSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;

    for (const auto& removedEntry : entries) {
        if (!_IsAncestorFilteredOutInclusive(removedEntry.primPath)) {
            removedEntries.emplace_back(removedEntry);
        } else {
            _filteredPrims.erase(removedEntry.primPath);
            for (auto it = _filteredPrims.begin(); it != _filteredPrims.end();) {
                if ((*it).HasPrefix(removedEntry.primPath)) {
                    it = _filteredPrims.erase(it);
                } else {
                    ++it;
                }
            }
        }
    }

    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
}

void PurposeFilteringSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        _UpdateFilteringForTree(entry.primPath);
        if (!_IsAncestorFilteredOutInclusive(entry.primPath)) {
            dirtiedEntries.emplace_back(entry);
        }
    }
    _SendPrimsDirtied(dirtiedEntries);
}

} // namespace FVP_NS_DEF
