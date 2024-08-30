//
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

#include <flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h>
#include <flowViewport/selection/fvpSelection.h>
#include <flowViewport/selection/fvpPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

#include "flowViewport/debugCodes.h"

#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const HdContainerDataSourceHandle visOff =
    HdVisibilitySchema::BuildRetained(
        HdRetainedTypedSampledDataSource<bool>::New(false));
}

namespace FVP_NS_DEF {

IsolateSelectSceneIndexRefPtr
IsolateSelectSceneIndex::New(
    const std::string&            viewportId,
    const SelectionPtr&           isolateSelection,
    const HdSceneIndexBaseRefPtr& inputSceneIndex
)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::New() called.\n");

    auto isSi = PXR_NS::TfCreateRefPtr(new IsolateSelectSceneIndex(
        viewportId, isolateSelection, inputSceneIndex));

    isSi->SetDisplayName(kDisplayName);
    return isSi;
}

IsolateSelectSceneIndex::IsolateSelectSceneIndex(
    const std::string&            viewportId,
    const SelectionPtr&           isolateSelection,
    const HdSceneIndexBaseRefPtr& inputSceneIndex
)
    : ParentClass(inputSceneIndex)
    , InputSceneIndexUtils(inputSceneIndex)
    , _viewportId(viewportId)
    , _isolateSelection(isolateSelection)
{}

std::string IsolateSelectSceneIndex::GetViewportId() const
{
    return _viewportId;
}

HdSceneIndexPrim IsolateSelectSceneIndex::GetPrim(const SdfPath& primPath) const
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::GetPrim(%s) called for viewport %s.\n", primPath.GetText(), _viewportId.c_str());

    auto inputPrim = GetInputSceneIndex()->GetPrim(primPath);

    // If isolate selection is empty, everything is included.
    if (_isolateSelection->IsEmpty()) {
        return inputPrim;
    }

    const bool included = 
        _isolateSelection->HasAncestorOrDescendantInclusive(primPath);

    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("    prim path %s is %s isolate select set", primPath.GetText(), (included ? "INCLUDED in" : "EXCLUDED from"));

    if (!included) {
        inputPrim.dataSource = HdContainerDataSourceEditor(inputPrim.dataSource)
            .Set(HdVisibilitySchema::GetDefaultLocator(), visOff)
            .Finish();
    }

    return inputPrim;
}

SdfPathVector IsolateSelectSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const {
    // Prims are hidden, not removed.
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void IsolateSelectSceneIndex::_PrimsAdded(
    const HdSceneIndexBase&                       ,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    // Prims outside the isolate select set will be hidden in GetPrim().
    _SendPrimsAdded(entries);
}

void IsolateSelectSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase&                         ,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    // We rely on the application to remove from the isolate select set those
    // prims that have been removed.
    _SendPrimsRemoved(entries);
}

void IsolateSelectSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase&                         ,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    _SendPrimsDirtied(entries);
}

void IsolateSelectSceneIndex::AddIsolateSelection(const PrimSelections& primSelections)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::AddIsolateSelection() called for viewport %s.\n", _viewportId.c_str());

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& primSelection : primSelections) {
        TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
            .Msg("    Adding %s to the isolate select set.\n", primSelection.primPath.GetText());
        _isolateSelection->Add(primSelection);
        _DirtyVisibility(primSelection.primPath, &dirtiedEntries);
    }

    _SendPrimsDirtied(dirtiedEntries);
}

void IsolateSelectSceneIndex::RemoveIsolateSelection(const PrimSelections& primSelections)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::RemoveIsolateSelection() called for viewport %s.\n", _viewportId.c_str());

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& primSelection : primSelections) {
        TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
            .Msg("    Removing %s from the isolate select set.\n", primSelection.primPath.GetText());
        _isolateSelection->Remove(primSelection);
        _DirtyVisibility(primSelection.primPath, &dirtiedEntries);
    }

    _SendPrimsDirtied(dirtiedEntries);
}

void IsolateSelectSceneIndex::ClearIsolateSelection()
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::ClearIsolateSelection() called for viewport %s.\n", _viewportId.c_str());

    auto isolateSelectPaths = _isolateSelection->GetFullySelectedPaths();

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& primPath : isolateSelectPaths) {
        TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
            .Msg("    Removing %s from the isolate select set.\n", primPath.GetText());
        _DirtyVisibility(primPath, &dirtiedEntries);
    }

    _isolateSelection->Clear();

    _SendPrimsDirtied(dirtiedEntries);
}

void IsolateSelectSceneIndex::ReplaceIsolateSelection(const SelectionConstPtr& isolateSelection)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::ReplaceIsolateSelection() called for viewport %s.\n", _viewportId.c_str());

    _ReplaceIsolateSelection(isolateSelection);

    _isolateSelection->Replace(*isolateSelection);
}

void IsolateSelectSceneIndex::_ReplaceIsolateSelection(const SelectionConstPtr& isolateSelection)
{
    // Keep paths in a set to minimize dirtying.  First clear old paths.
    auto clearedPaths = _isolateSelection->GetFullySelectedPaths();
    std::set<SdfPath> dirtyPaths(clearedPaths.begin(), clearedPaths.end());

    // Then add new paths.
    const auto& newPaths = isolateSelection->GetFullySelectedPaths();
    for (const auto& primPath : newPaths) {
        dirtyPaths.insert(primPath);
    }

    // Finally, dirty all cleared and added prim paths.
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& primPath : dirtyPaths) {
        _DirtyVisibility(primPath, &dirtiedEntries);
    }

    _SendPrimsDirtied(dirtiedEntries);
}

void IsolateSelectSceneIndex::SetViewport(
    const std::string&  viewportId,
    const SelectionPtr& isolateSelection
)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::SetViewport() called for new viewport %s.\n", viewportId.c_str());
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("    Old viewport was %s.\n", _viewportId.c_str());
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("    Old selection is %p, new selection is %p.\n", &*_isolateSelection, &*isolateSelection);

    if ((isolateSelection == _isolateSelection) ||
        (viewportId == _viewportId)) {
        TF_WARN("IsolateSelectSceneIndex::SetViewport() called with identical information, no operation performed.");
        return;
    }

    _ReplaceIsolateSelection(isolateSelection);

    _isolateSelection = isolateSelection;
    _viewportId = viewportId;
}

SelectionPtr IsolateSelectSceneIndex::GetIsolateSelection() const
{
    return _isolateSelection;
}

void IsolateSelectSceneIndex::_DirtyVisibility(
    const SdfPath&                            primPath,
    HdSceneIndexObserver::DirtiedPrimEntries* dirtiedEntries
) const
{
    // Dirty visibility by going up the prim path.  GetAncestorsRange()
    // includes the path itself, as desired.
    for (const auto& p : primPath.GetAncestorsRange()) {
        TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
            .Msg("    %s: examining %s for isolate select dirtying.\n", _viewportId.c_str(), p.GetText());
        if (p.GetPathElementCount() == 0) {
            break;
        }
        auto parent = p.GetParentPath();
        auto siblings = GetChildPrimPaths(parent);
        for (const auto& s : siblings) {
            if (s == p) {
                continue;
            }
            TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
                .Msg("        %s: dirtying sibling %s for isolate select.\n", _viewportId.c_str(), s.GetText());
            _DirtyVisibilityRecursive(s, dirtiedEntries);
        }
    }
}

void IsolateSelectSceneIndex::_DirtyVisibilityRecursive(
    const SdfPath&                            primPath, 
    HdSceneIndexObserver::DirtiedPrimEntries* dirtiedEntries
) const
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("            %s: marking %s visibility locator dirty.\n", _viewportId.c_str(), primPath.GetText());

    dirtiedEntries->emplace_back(
        primPath, HdVisibilitySchema::GetDefaultLocator());

    for (const auto& childPath : GetChildPrimPaths(primPath)) {
        _DirtyVisibilityRecursive(childPath, dirtiedEntries);
    }
}

} //end of namespace FVP_NS_DEF
