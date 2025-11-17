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

#include "fvpPrimRemovalEnforcingSceneIndex.h"

#include <pxr/imaging/hd/sceneIndexPrimView.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {
    
PrimRemovalEnforcingSceneIndexRefPtr PrimRemovalEnforcingSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex)
{
    // If we return TfCreateRefPtr directly, Clang will destroy the rvalue before
    // returning, which means we will return a null pointer. To avoid this, store 
    // the pointer in an lvalue first and return that.
    auto refPtr = TfCreateRefPtr(new PrimRemovalEnforcingSceneIndex(inputSceneIndex));
    return refPtr;
}

PrimRemovalEnforcingSceneIndex::PrimRemovalEnforcingSceneIndex(HdSceneIndexBaseRefPtr const& inputSceneIndex)
    : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
{
    for (const SdfPath& primPath : HdSceneIndexPrimView(GetInputSceneIndex())) {
        // Assume all paths are actual prims initially. Even if they're not, we're
        // enforcing removal here, and when that happens, the paths will be removed
        // from the hierarchy map, so this won't matter anymore.
        _hierarchy[primPath] = true;
    }
}

bool PrimRemovalEnforcingSceneIndex::_PrimExists(const PXR_NS::SdfPath& primPath) const
{
    auto it = _hierarchy.find(primPath);
    return it != _hierarchy.end() ? it->second : false;
}

bool PrimRemovalEnforcingSceneIndex::_PathExists(const PXR_NS::SdfPath& primPath) const
{
    auto it = _hierarchy.find(primPath);
    return it != _hierarchy.end();
}

HdSceneIndexPrim PrimRemovalEnforcingSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (!_PrimExists(primPath)) {
        return {};
    }
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PrimRemovalEnforcingSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    if (!_PathExists(primPath)) {
        return {};
    }
    auto childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    SdfPathVector editedChildPaths;
    for (const auto& childPath : childPaths) {
        if (_PathExists(childPath)) {
            editedChildPaths.emplace_back(childPath);
        }
    }
    return editedChildPaths;
}

void PrimRemovalEnforcingSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    for (const auto& addedEntry : entries) {
        _hierarchy[addedEntry.primPath] = true;
    }
    if (!entries.empty()) {
        _SendPrimsAdded(entries);
    }
}

void PrimRemovalEnforcingSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    for (const auto& removedEntry : entries) {
        // As _hierarchy is an SdfPathTable, all descendents will also
        // be automatically removed from the table.
        _hierarchy.erase(removedEntry.primPath);
    }
    if (!entries.empty()) {
        _SendPrimsRemoved(entries);
    }
}

void PrimRemovalEnforcingSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        // Avoid sending notifications on non-existent prims
        if (_PrimExists(entry.primPath)) {
            dirtiedEntries.push_back(entry);
        }
    }
    if (!dirtiedEntries.empty()) {
        _SendPrimsDirtied(dirtiedEntries);
    }
}

} // namespace FVP_NS_DEF
