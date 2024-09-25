//
// Copyright 2022 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
//
// Copyright 2023 Autodesk
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

#include "flowViewport/selection/fvpSelection.h"

#include "flowViewport/fvpUtils.h"

#include "pxr/imaging/hd/selectionsSchema.h"

#include <memory>               // std::shared_ptr

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

/* static */
SelectionPtr Selection::New()
{
    return std::make_shared<Selection>();
}

bool
Selection::Add(const PrimSelection& primSelection)
{
    if (primSelection.primPath.IsEmpty()) {
        return false;
    }

    _pathToSelections[primSelection.primPath].push_back(primSelection);

    return true;
}

bool Selection::Remove(const PrimSelection& primSelection)
{
    if (primSelection.primPath.IsEmpty()) {
        return false;
    }

    // Remove the specific selection
    auto itSelection = std::find(
        _pathToSelections[primSelection.primPath].begin(), 
        _pathToSelections[primSelection.primPath].end(),
        primSelection);
    if (itSelection != _pathToSelections[primSelection.primPath].end()) {
        _pathToSelections[primSelection.primPath].erase(itSelection);
    }

    // If no selections remain, remove the entry entirely
    if (_pathToSelections[primSelection.primPath].empty()) {
        _pathToSelections.erase(primSelection.primPath);
    }

    return true;
}

void
Selection::Clear()
{
    _pathToSelections.clear();
}

void Selection::Replace(const PrimSelections& primSelections)
{
    Clear();

    for (const auto& primSelection : primSelections) {
        if (primSelection.primPath.IsEmpty()) {
            continue;
        }
        _pathToSelections[primSelection.primPath].push_back(primSelection);
    }
}

void Selection::Replace(const Selection& rhs)
{
    _pathToSelections = rhs._pathToSelections;
}

void Selection::RemoveHierarchy(const PXR_NS::SdfPath& primPath)
{
    auto it = _pathToSelections.lower_bound(primPath);
    while (it != _pathToSelections.end() && it->first.HasPrefix(primPath)) {
        it = _pathToSelections.erase(it);
    }
}

bool Selection::IsEmpty() const
{
    return _pathToSelections.empty();
}

bool Selection::IsFullySelected(const SdfPath& primPath) const
{
    return _pathToSelections.find(primPath) != _pathToSelections.end()
        && !_pathToSelections.at(primPath).empty();
}

bool Selection::HasFullySelectedAncestorInclusive(const SdfPath& primPath, const SdfPath& topmostAncestor/* = SdfPath::AbsoluteRootPath()*/) const
{
    // FLOW_VIEWPORT_TODO  Prefix tree would be much higher performance
    // than iterating over the whole selection, especially for a large
    // selection.  PPT, 13-Sep-2023.
    for(const auto& entry : _pathToSelections) {
        if (primPath.HasPrefix(entry.first) && entry.first.HasPrefix(topmostAncestor)) {
            return true;
        }
    }
    return false;
}

bool Selection::HasDescendantInclusive(const PXR_NS::SdfPath& primPath) const
{
    // No entries?  No descendant
    if (_pathToSelections.empty()) {
        return false;
    }

    // At least one entry.  Skip all entries before argument.  The iterator
    // points to an entry with matching or greater key.
    auto it = _pathToSelections.lower_bound(primPath);

    // Reached the end?  Last entry is strictly smaller than, so no descendants.
    if (it == _pathToSelections.end()) {
        return false;
    }

    // Not at the end.  Query is exactly in the map, or is prefix to what is in
    // the map (i.e. map contents is a descendant)?  Return true.
    return (it->first == primPath || it->first.HasPrefix(primPath));
}

bool 
Selection::HasAncestorOrDescendantInclusive(const PXR_NS::SdfPath& primPath) const
{
    // Use std::map::lower_bound to accelerate prim path lookup. The map is
    // lexically ordered on SdfPath, with shorter paths less than longer
    // paths. Makes determining ancestors and descendants somewhat tricky, but
    // efficient. A prefix tree would be an easier data structure to implement
    // this functionality.

    // No entries?  No ancestors or descendants.
    if (_pathToSelections.empty()) {
        return false;
    }

    // At least one entry.  Skip all entries before argument.  The iterator
    // points to an entry with matching or greater key.
    auto it = _pathToSelections.lower_bound(primPath);

    // Reached the end?  Last entry is strictly smaller than, so no descendants
    // in map.  Check if it's an ancestor.
    if (it == _pathToSelections.end()) {
        auto rit = _pathToSelections.rbegin();
        return primPath.HasPrefix(rit->first);
    }

    // Not at the end.  Map entry has matching or greater key, so check 
    // match, or if the map entry with greater key is a descendant (i.e. query
    // is an ancestor).
    if (it->first == primPath ||         // Query is in map
        it->first.HasPrefix(primPath)) { // Query descendant in map
            return true;
    }

    // Map entry is strictly greater and not a descendant.  For the map entry
    // to be an ancestor of the query, it would have to be less than the query.
    // Thus, if we're at the beginning, the map entry is unrelated to the query.
    if (it == _pathToSelections.begin()) {
        return false;
    }

    // Map entry still strictly greater and not a descendant.  Is the previous
    // map entry (lower key) a prefix (ancestor) to the query (i.e. query is a
    // descendant)?
    it = std::prev(it);
    return primPath.HasPrefix(it->first); // Ancestor in map
}

SdfPathVector Selection::FindFullySelectedAncestorsInclusive(const SdfPath& primPath, const SdfPath& topmostAncestor/* = SdfPath::AbsoluteRootPath()*/) const
{
    // FLOW_VIEWPORT_TODO  Prefix tree would be much higher performance
    // than iterating over the whole selection, especially for a large
    // selection.  PPT, 13-Sep-2023.
    SdfPathVector fullySelectedAncestors;
    for(const auto& entry : _pathToSelections) {
        if (primPath.HasPrefix(entry.first) && entry.first.HasPrefix(topmostAncestor)) {
            fullySelectedAncestors.push_back(entry.first);
        }
    }
    return fullySelectedAncestors;
}

SdfPathVector Selection::GetFullySelectedPaths() const
{
    SdfPathVector fullySelectedPaths;
    fullySelectedPaths.reserve(_pathToSelections.size());
    for(const auto& entry : _pathToSelections) {
        fullySelectedPaths.emplace_back(entry.first);
    }
    return fullySelectedPaths;
}

HdDataSourceBaseHandle Selection::GetVectorDataSource(
    const PXR_NS::SdfPath& primPath
) const
{
    auto it = _pathToSelections.find(primPath);
    if (it == _pathToSelections.end()) {
        return nullptr;
    }

    std::vector<HdDataSourceBaseHandle> selectionDataSources;
    for (const auto& selection : it->second) {
        selectionDataSources.push_back(createSelectionDataSource(selection));
    }
    return HdSelectionsSchema::BuildRetained(
        selectionDataSources.size(), selectionDataSources.data()
    );
}

}
