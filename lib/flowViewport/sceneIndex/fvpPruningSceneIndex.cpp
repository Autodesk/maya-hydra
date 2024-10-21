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

#include "fvpPruningSceneIndex.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/tokens.h>

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

bool _MeshesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    // Currently we just flat out remove any prim with a mesh type. If we were to add extra checks to make sure this is not
    // a mesh prim that serves another purpose, we would add them here.
    return prim.primType == HdPrimTypeTokens->mesh;
}

bool _CapsulesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->capsule;
}

bool _ConesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->cone;
}

bool _CubesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->cube;
}

bool _CylindersFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->cylinder;
}

bool _SpheresFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->sphere;
}

bool _NurbsCurvesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->nurbsCurves;
}

bool _NurbsPatchesFilterHandler(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& primPath, const HdSceneIndexPrim& prim)
{
    return prim.primType == HdPrimTypeTokens->nurbsPatch;
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
    using FilterHandler = std::function<bool(const HdSceneIndexBaseRefPtr&, const SdfPath&, const HdSceneIndexPrim&)>;
    static std::map<TfToken, FilterHandler> filterHandlers = {
        { FvpPruningTokens->meshes, _MeshesFilterHandler },
        { FvpPruningTokens->capsules, _CapsulesFilterHandler },
        { FvpPruningTokens->cones, _ConesFilterHandler },
        { FvpPruningTokens->cubes, _CubesFilterHandler },
        { FvpPruningTokens->cylinders, _CylindersFilterHandler },
        { FvpPruningTokens->spheres, _SpheresFilterHandler },
        { FvpPruningTokens->nurbsCurves, _NurbsCurvesFilterHandler },
        { FvpPruningTokens->nurbsPatches, _NurbsPatchesFilterHandler }
    };
    return filterHandlers[pruningToken](GetInputSceneIndex(), primPath, prim);
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

    // Enable the filter
    _prunedPathsByFilter[pruningToken] = SdfPathSet();

    HdSceneIndexObserver::RemovedPrimEntries prunedPrims;

    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        if (_PrunePrim(primPath, GetInputSceneIndex()->GetPrim(primPath), pruningToken)) {
            if (!_IsAncestorPrunedInclusive(primPath)) {
                // Only send notification if it was not already pruned out, directly or indirectly
                prunedPrims.emplace_back(primPath);
            }
            _InsertEntry(primPath, pruningToken);
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

    HdSceneIndexObserver::AddedPrimEntries unprunedPrims;

    SdfPathSet prunedPaths = _prunedPathsByFilter[pruningToken];
    for (const auto& primPath : prunedPaths) {
        _RemoveEntry(primPath, pruningToken);
        if (!_IsAncestorPrunedInclusive(primPath)) {
            // Only send notification if it was pruned and no longer is
            unprunedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
        }
    }

    // Disable the filter
    _prunedPathsByFilter.erase(pruningToken);

    if (!unprunedPrims.empty()) {
        _SendPrimsAdded(unprunedPrims);
    }

    return true;
}

std::set<TfToken> PruningSceneIndex::GetActiveFilters()
{
    std::set<TfToken> pruningTokens;
    for (const auto& filterEntry : _prunedPathsByFilter) {
        pruningTokens.emplace(filterEntry.first);
    }
    return pruningTokens;
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
        for (const auto& pruningToken : GetActiveFilters()) {
            if (_PrunePrim(addedEntry.primPath, GetInputSceneIndex()->GetPrim(addedEntry.primPath), pruningToken)) {
                _InsertEntry(addedEntry.primPath, pruningToken);
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
        if (!_IsAncestorPrunedInclusive(removedEntry.primPath)) {
            editedEntries.emplace_back(removedEntry);
        } else {
            for (const auto& pruningToken : GetActiveFilters()) {
                _RemoveEntry(removedEntry.primPath, pruningToken);
            }
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

        for (const auto& pruningToken : GetActiveFilters()) {
            if (_PrunePrim(dirtiedEntry.primPath, dirtiedPrim, pruningToken)) {
                _InsertEntry(dirtiedEntry.primPath, pruningToken);
            } else {
                _RemoveEntry(dirtiedEntry.primPath, pruningToken);
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
