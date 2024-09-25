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

bool disabled(const Fvp::SelectionConstPtr& a, const Fvp::SelectionConstPtr& b)
{
    return !a && !b;
}

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

    // If there is no isolate selection, everything is included.
    if (!_isolateSelection) {
        return inputPrim;
    }

    // If isolate selection is empty, then nothing is included (everything
    // is excluded), as desired.
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

    if (!TF_VERIFY(_isolateSelection, "AddIsolateSelection() called for viewport %s while isolate selection is disabled", _viewportId.c_str())) {
        return;
    }

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

    if (!TF_VERIFY(_isolateSelection, "RemoveIsolateSelection() called for viewport %s while isolate selection is disabled", _viewportId.c_str())) {
        return;
    }

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

    if (!TF_VERIFY(_isolateSelection, "ClearIsolateSelection() called for viewport %s while isolate selection is disabled", _viewportId.c_str())) {
        return;
    }

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

void IsolateSelectSceneIndex::ReplaceIsolateSelection(const SelectionConstPtr& newIsolateSelection)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::ReplaceIsolateSelection() called for viewport %s.\n", _viewportId.c_str());

    if (!TF_VERIFY(_isolateSelection, "ReplaceIsolateSelection() called for viewport %s while isolate selection is disabled", _viewportId.c_str())) {
        return;
    }

    if (!TF_VERIFY(newIsolateSelection, "ReplaceIsolateSelection() called for viewport %s with illegal null isolate selection pointer", _viewportId.c_str())) {
        return;
    }

    _ReplaceIsolateSelection(newIsolateSelection);

    _isolateSelection->Replace(*newIsolateSelection);
}

void IsolateSelectSceneIndex::_ReplaceIsolateSelection(const SelectionConstPtr& newIsolateSelection)
{
    // Trivial case of going from disabled to disabled is an early out.
    if (disabled(_isolateSelection, newIsolateSelection)) {
        return;
    }

    // If the old and new isolate selection are equal, nothing to do.  Only
    // handling trivial case of empty isolate select (i.e. hide everything) for
    // the moment.
    if ((_isolateSelection && _isolateSelection->IsEmpty()) &&
        (newIsolateSelection && newIsolateSelection->IsEmpty())) {
        return;
    }
        
    // Keep paths in a set to minimize dirtying.
    std::set<SdfPath> dirtyPaths;

    // First clear old paths.  If we were disabled do nothing.  If there were
    // no selected paths the whole scene was invisible.
    _InsertSelectedPaths(_isolateSelection, dirtyPaths);

    // Then add new paths.  If we're disabled do nothing.  If there are no
    // selected paths the whole scene is invisible.
    _InsertSelectedPaths(newIsolateSelection, dirtyPaths);

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;

    // Dirty all cleared and added prim paths.
    for (const auto& primPath : dirtyPaths) {
        _DirtyVisibility(primPath, &dirtiedEntries);
    }

    _SendPrimsDirtied(dirtiedEntries);
}

void IsolateSelectSceneIndex::_InsertSelectedPaths(
    const SelectionConstPtr& selection,
    std::set<SdfPath>&       dirtyPaths
)
{
    if (!selection) {
        return;
    }
    const auto& paths = selection->IsEmpty() ? 
        GetChildPrimPaths(SdfPath::AbsoluteRootPath()) :
        selection->GetFullySelectedPaths();
    for (const auto& primPath : paths) {
        dirtyPaths.insert(primPath);
    }
}

void IsolateSelectSceneIndex::SetViewport(
    const std::string&  viewportId,
    const SelectionPtr& newIsolateSelection
)
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("IsolateSelectSceneIndex::SetViewport() called for new viewport %s.\n", viewportId.c_str());
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("    Old viewport was %s.\n", _viewportId.c_str());
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("    Old selection is %p, new selection is %p.\n", (_isolateSelection ? &*_isolateSelection : (void*) 0), (newIsolateSelection ? &*newIsolateSelection : (void*) 0));

    if ((newIsolateSelection == _isolateSelection) &&
        (viewportId == _viewportId)) {
        TF_WARN("IsolateSelectSceneIndex::SetViewport() called with identical information, no operation performed.");
        return;
    }

    // If the previous and new viewports both had isolate select disabled,
    // no visibility to dirty.
    if (disabled(_isolateSelection, newIsolateSelection)) {
        _viewportId = viewportId;
        return;
    }

    _ReplaceIsolateSelection(newIsolateSelection);

    _isolateSelection = newIsolateSelection;
    _viewportId = viewportId;
}

void IsolateSelectSceneIndex::SetIsolateSelection(
    const SelectionPtr& newIsolateSelection
)
{
    _isolateSelection = newIsolateSelection;
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
