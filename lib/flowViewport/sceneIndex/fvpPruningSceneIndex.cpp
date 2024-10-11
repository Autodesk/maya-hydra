// Copyright 2024 Autodesk
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

#include "flowViewport/sceneIndex/fvpPruningSceneIndex.h"
#include "fvpPruningSceneIndex.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/path.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(FvpPruningTokens, FVP_PRUNING_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

template<typename Container>
bool _HasAncestorInclusiveInContainer(const SdfPath& path, const Container& pathsContainer) {
    SdfPath currPath = path;
    while (!currPath.IsEmpty() && !currPath.IsAbsoluteRootPath()) {
        if (pathsContainer.find(currPath) != pathsContainer.end()) {
            return true;
        } else {
            currPath = currPath.GetParentPath();
        }
    }
    return false;
}

} // namespace

namespace FVP_NS_DEF {

PruningSceneIndexRefPtr PruningSceneIndex::New(const HdSceneIndexBaseRefPtr &inputSceneIndex)
{
    return TfCreateRefPtr(new PruningSceneIndex(inputSceneIndex));
}

PruningSceneIndex::PruningSceneIndex(HdSceneIndexBaseRefPtr const &inputSceneIndex) : 
    HdSingleInputFilteringSceneIndexBase(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex)
{
}

void PruningSceneIndex::AddExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot)
{
    _excludedSceneRoots.emplace(sceneRoot);
}

bool PruningSceneIndex::_IsExcluded(const PXR_NS::SdfPath& primPath) const
{
    return _HasAncestorInclusiveInContainer(primPath, _excludedSceneRoots);
}

bool PruningSceneIndex::_PrunePrim(const SdfPath& primPath, const HdSceneIndexPrim& prim, const TfToken& pruningToken) const
{
    if (_IsExcluded(primPath)) {
        return false;
    }
    if (pruningToken == FvpPruningTokens->mesh) {
        return prim.primType == HdPrimTypeTokens->mesh;
    }
    return false;
}

bool PruningSceneIndex::_IsAncestorPrunedInclusive(const SdfPath& primPath) const
{
    return _HasAncestorInclusiveInContainer(primPath, _filtersByPrunedPath);
}

HdSceneIndexPrim PruningSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (_filtersByPrunedPath.find(primPath) != _filtersByPrunedPath.end()) {
        return {};
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PruningSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    SdfPathVector baseChildPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector editedChildPaths;
    for (const auto& baseChildPath : baseChildPaths) {
        if (_filtersByPrunedPath.find(baseChildPath) == _filtersByPrunedPath.end()) {
            editedChildPaths.emplace_back(baseChildPath);
        }
    }
    return editedChildPaths;
}

bool PruningSceneIndex::EnableFilter(const TfToken& pruningToken)
{
    if (_prunedPathsByFilter.find(pruningToken) != _prunedPathsByFilter.end()) {
        // Filter already enabled, no change needed.
        return false;
    }

    HdSceneIndexObserver::RemovedPrimEntries prunedPrims;

    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        if (_PrunePrim(primPath, GetInputSceneIndex()->GetPrim(primPath), pruningToken)) {
            if (!_IsAncestorPrunedInclusive(primPath)) {
                // Only send notification if it was not already pruned out, directly or indirectly
                prunedPrims.emplace_back(primPath);
            }
            _prunedPathsByFilter[pruningToken].emplace(primPath);
            _filtersByPrunedPath[primPath].emplace(pruningToken);
        }
    }

    if (!prunedPrims.empty()) {
        _SendPrimsRemoved(prunedPrims);
    }

    return true;
}

bool PruningSceneIndex::DisableFilter(const TfToken& pruningToken)
{
    if (_prunedPathsByFilter.find(pruningToken) == _prunedPathsByFilter.end()) {
        // Filter already disabled, no change needed.
        return false;
    }

    SdfPathSet prunedPaths = _prunedPathsByFilter[pruningToken];

    _prunedPathsByFilter.erase(pruningToken);
    for (const auto& primPath : prunedPaths) {
        _filtersByPrunedPath[primPath].erase(pruningToken);
        if (_filtersByPrunedPath[primPath].empty()) {
            _filtersByPrunedPath.erase(primPath);
        }
    }

    HdSceneIndexObserver::AddedPrimEntries unprunedPrims;
    for (const auto& primPath : prunedPaths) {
        // If it's still pruned, avoid sending a notification
        if (!_IsAncestorPrunedInclusive(primPath)) {
            unprunedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
        }
    }

    if (!unprunedPrims.empty()) {
        _SendPrimsAdded(unprunedPrims);
    }

    return true;
}

void PruningSceneIndex::_InsertEntry(const PXR_NS::SdfPath& primPath, const PXR_NS::TfToken& pruningToken)
{
    _prunedPathsByFilter[pruningToken].emplace(primPath);
    _filtersByPrunedPath[primPath].emplace(pruningToken);
}

void PruningSceneIndex::_RemoveEntry(const PXR_NS::SdfPath& primPath, const PXR_NS::TfToken& pruningToken)
{
    if (_prunedPathsByFilter.find(pruningToken) != _prunedPathsByFilter.end()) {
        _prunedPathsByFilter[pruningToken].erase(primPath);
    }

    if (_filtersByPrunedPath.find(primPath) != _filtersByPrunedPath.end()) {
        _filtersByPrunedPath[primPath].erase(pruningToken);
        if (_filtersByPrunedPath[primPath].empty()) {
            _filtersByPrunedPath.erase(primPath);
        }
    }
}

void PruningSceneIndex::_PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries editedEntries;
    for (const auto& addedEntry : entries) {
        for (auto& filterEntry : _prunedPathsByFilter) {
            if (_PrunePrim(addedEntry.primPath, GetInputSceneIndex()->GetPrim(addedEntry.primPath), filterEntry.first)) {
                _InsertEntry(addedEntry.primPath, filterEntry.first);
            }
        }
        if (!_IsAncestorPrunedInclusive(addedEntry.primPath)) {
            editedEntries.emplace_back(addedEntry);
        }
    }
    if (!editedEntries.empty()) {
        _SendPrimsAdded(editedEntries);
    }
}

void PruningSceneIndex::_PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries editedEntries;
    for (const auto& removedEntry : entries) {
        for (auto& filterEntry : _prunedPathsByFilter) {
            _RemoveEntry(removedEntry.primPath, filterEntry.first);
        }
        if (!_IsAncestorPrunedInclusive(removedEntry.primPath)) {
            editedEntries.emplace_back(removedEntry);
        }
    }
    if (!editedEntries.empty()) {
        _SendPrimsRemoved(editedEntries);
    }
}

void PruningSceneIndex::_PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;
    HdSceneIndexObserver::AddedPrimEntries addedEntries;
    HdSceneIndexObserver::DirtiedPrimEntries editedEntries;

    for (const auto& dirtiedEntry : entries) {
        bool wasInitiallyPruned = _IsAncestorPrunedInclusive(dirtiedEntry.primPath);
        HdSceneIndexPrim dirtiedPrim = GetInputSceneIndex()->GetPrim(dirtiedEntry.primPath);
        for (auto& filterEntry : _prunedPathsByFilter) {
            if (_PrunePrim(dirtiedEntry.primPath, dirtiedPrim, filterEntry.first)) {
                _InsertEntry(dirtiedEntry.primPath, filterEntry.first);
            } else {
                _RemoveEntry(dirtiedEntry.primPath, filterEntry.first);
            }
        }
        bool isNowPruned = _IsAncestorPrunedInclusive(dirtiedEntry.primPath);
        if (!wasInitiallyPruned && isNowPruned) {
            removedEntries.emplace_back(dirtiedEntry.primPath);
        } else if (wasInitiallyPruned && !isNowPruned) {
            addedEntries.emplace_back(dirtiedEntry.primPath, dirtiedPrim.primType);
        } else {
            editedEntries.emplace_back(dirtiedEntry);
        }
    }

    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
    if (!addedEntries.empty()) {
        _SendPrimsAdded(addedEntries);
    }
    if (!editedEntries.empty()) {
        _SendPrimsDirtied(editedEntries);
    }
}

} // namespace FVP_NS_DEF
