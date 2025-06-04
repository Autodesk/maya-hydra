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

#include "fvpPassFilteringSceneIndex.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

PassFilteringSceneIndexRefPtr PassFilteringSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const FilteringOutFn& filteringOutFn)
{
    return TfCreateRefPtr(new PassFilteringSceneIndex(inputSceneIndex, filteringOutFn));
}

PassFilteringSceneIndex::PassFilteringSceneIndex(
    HdSceneIndexBaseRefPtr const& inputSceneIndex,
    const FilteringOutFn& filteringOutFn)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _filteringOutFn(filteringOutFn)
{
}

bool PassFilteringSceneIndex::_IsFilteredOut(const PXR_NS::SdfPath& primPath) const
{
    if (_filteringOutFn) {
        return _filteringOutFn(primPath);
    }
    return false;
}

HdSceneIndexPrim PassFilteringSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (_IsFilteredOut(primPath)) {
        return {};
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PassFilteringSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    SdfPathVector baseChildPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector editedChildPaths;
    for (const auto& baseChildPath : baseChildPaths) {
        if (!_IsFilteredOut(baseChildPath)) {
            editedChildPaths.emplace_back(baseChildPath);
        }
    }
    return editedChildPaths;
}

void PassFilteringSceneIndex::_PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries)
{
    HdSceneIndexObserver::AddedPrimEntries editedEntries;

    for (const auto& addedEntry : entries) {
        if (!_IsFilteredOut(addedEntry.primPath)) {
            editedEntries.emplace_back(addedEntry);
        }
    }

    if (!editedEntries.empty()) {
        _SendPrimsAdded(editedEntries);
    }
}

void PassFilteringSceneIndex::_PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    HdSceneIndexObserver::RemovedPrimEntries removedEntries;

    for (const auto& removedEntry : entries) {
        if (!_IsFilteredOut(removedEntry.primPath)) {
            removedEntries.emplace_back(removedEntry);
        }
    }

    if (!removedEntries.empty()) {
        _SendPrimsRemoved(removedEntries);
    }
}

void PassFilteringSceneIndex::_PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;

    for (const auto& dirtiedEntry : entries) {
        if (!_IsFilteredOut(dirtiedEntry.primPath)) {
            dirtiedEntries.emplace_back(dirtiedEntry);
        }
    }

    if (!dirtiedEntries.empty()) {
        _SendPrimsDirtied(dirtiedEntries);
    }
}

} // namespace FVP_NS_DEF
