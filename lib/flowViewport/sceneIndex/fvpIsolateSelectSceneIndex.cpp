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
#include <pxr/imaging/hd/instanceSchema.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

using Dependencies = TfSmallVector<SdfPath, 8>;

const HdContainerDataSourceHandle visOff =
    HdVisibilitySchema::BuildRetained(
        HdRetainedTypedSampledDataSource<bool>::New(false));

const HdDataSourceLocator instancerMaskLocator(
    HdInstancerTopologySchemaTokens->instancerTopology,
    HdInstancerTopologySchemaTokens->mask
);

bool disabled(const Fvp::SelectionConstPtr& a, const Fvp::SelectionConstPtr& b)
{
    return !a && !b;
}

void append(Fvp::Selection& a, const Fvp::Selection& b)
{
    for (const auto& entry : b) {
        const auto& primSelections = entry.second;
        for (const auto& primSelection : primSelections) {
            a.Add(primSelection);
        }
    }
}

std::size_t getNbInstances(const HdInstancerTopologySchema& instancerTopologySchema)
{
    // No easy way to get the number of instances created by a point instancer.
    // Count the total number of instances for all prototypes.  As per documentation
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/v24.08/pxr/imaging/hd/instancerTopologySchema.h
    // the instance indices are a per-prototype array of instance index arrays,
    // so counting the size of all instance index array gives the total number
    // of instances.  For example, if instanceIndices = { [0,2], [1] },
    // prototype 0 has two instances, and prototype 1 has one, for a total of 3.
    std::size_t nbInstances{0};
    auto instanceIndices = instancerTopologySchema.GetInstanceIndices();
    for (std::size_t i = 0; i < instanceIndices.GetNumElements(); ++i) {
        const auto& protoInstances = instanceIndices.GetElement(i)->GetTypedValue(0);
        nbInstances += protoInstances.size();
    }
    return nbInstances;
}

Dependencies instancedPrim(
    const Fvp::IsolateSelectSceneIndex& si,
    const SdfPath&                      primPath
)
{
    auto prim = si.GetInputSceneIndex()->GetPrim(primPath);
    auto instanceSchema = HdInstanceSchema::GetFromParent(prim.dataSource);
    return (instanceSchema.IsDefined() ? Dependencies{{
                instanceSchema.GetInstancer()->GetTypedValue(0)}} : 
        Dependencies());
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

    auto instancerMask = _instancerMasks.find(primPath);
    if (instancerMask != _instancerMasks.end()) {
        auto maskDs = HdRetainedTypedSampledDataSource<VtArray<bool>>::New(instancerMask->second);

        inputPrim.dataSource = HdContainerDataSourceEditor(inputPrim.dataSource)
            .Set(instancerMaskLocator, maskDs)
            .Finish();

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

    _DirtyIsolateSelection(newIsolateSelection);

    _isolateSelection->Replace(*newIsolateSelection);
}

void IsolateSelectSceneIndex::_DirtyIsolateSelection(const SelectionConstPtr& newIsolateSelection)
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

    // Add dependencies of the new isolate selection to protect them from being
    // marked as invisible.
    _AddDependencies(newIsolateSelection);

    _DirtyIsolateSelection(newIsolateSelection);

    // Collect all the instancers from the new isolate selection.
    auto instancers = _CollectInstancers(newIsolateSelection);

    // Create the instance mask for each instancer.
    auto newInstancerMasks = _CreateInstancerMasks(instancers, newIsolateSelection);

    // Dirty the instancer masks.
    _DirtyInstancerMasks(newInstancerMasks);

    _isolateSelection = newIsolateSelection;
    _instancerMasks = newInstancerMasks;
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

void IsolateSelectSceneIndex::_AddDependencies(
    const SelectionPtr& isolateSelection
)
{
    // Iterate over the input isolate selection, and find the dependencies.
    // As of 27-Sep-2024 only instancer dependencies are supported.
    if (!isolateSelection) {
        return;
    }

    // Collect dependencies in this selection.
    Selection dependencies;
    for (const auto& primSelectionsEntry : *isolateSelection) {
        for (const auto& primSelection : primSelectionsEntry.second) {
            auto primDependencies = instancedPrim(*this, primSelection.primPath);
            for (const auto& dependencyPath : primDependencies) {
                dependencies.Add(PrimSelection{dependencyPath});
            }
        }
    }

    // Add the collected dependencies to the input isolate selection.
    append(*isolateSelection, dependencies);
}

IsolateSelectSceneIndex::Instancers
IsolateSelectSceneIndex::_CollectInstancers(
    const SelectionConstPtr& isolateSelection
) const
{
    if (!isolateSelection) {
        return {};
    }

    Instancers instancers;
    for (const auto& primSelectionsEntry : *isolateSelection) {
        // If the prim itself is a point instancer, add it and continue.
        if (_IsPointInstancer(primSelectionsEntry.first)) {
            instancers.emplace_back(primSelectionsEntry.first);
            continue;
        }

        for (const auto& primSelection : primSelectionsEntry.second) {
            auto prim = GetInputSceneIndex()->GetPrim(primSelection.primPath);
            auto instanceSchema = HdInstanceSchema::GetFromParent(prim.dataSource);
            if (instanceSchema.IsDefined()) {
                instancers.emplace_back(instanceSchema.GetInstancer()->GetTypedValue(0));
            }
        }
    }

    return instancers;
}

bool IsolateSelectSceneIndex::_IsPointInstancer(
    const PXR_NS::SdfPath& primPath
) const
{
    auto prim = GetInputSceneIndex()->GetPrim(primPath);

    // If prim isn't an instancer, it can't be a point instancer.
    if (prim.primType != HdPrimTypeTokens->instancer) {
        return false;
    }

    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(prim.dataSource);

    // Documentation
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instancerTopologySchema.h#L86
    // says that instanceLocations is only meaningful for native
    // instancing, empty for point instancing.
    auto instanceLocationsDs = instancerTopologySchema.GetInstanceLocations();
    if (!instanceLocationsDs) {
        return true;
    }

    auto instanceLocations = instanceLocationsDs->GetTypedValue(0.0f);

    return (instanceLocations.size() > 0);
}

IsolateSelectSceneIndex::InstancerMasks
IsolateSelectSceneIndex::_CreateInstancerMasks(
    const Instancers&        instancers,
    const SelectionConstPtr& isolateSelection
) const
{
    // If isolate select is disabled, no instancer masks to compute.
    if (!isolateSelection) {
        return {};
    }

    // For each instancer, build its mask of visible instances by running all
    // instances, given by instancerTopology.instanceLocations, through
    // the isolate selection.  This determines whether the instance is visible
    // or not.  Store the instancer mask for the instancer path.
    InstancerMasks instancerMasks;
    for (const auto& instancerPath : instancers) {
        HdSceneIndexPrim instancerPrim = GetInputSceneIndex()->GetPrim(instancerPath);
        auto instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(instancerPrim.dataSource);

        // Documentation
        // https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instancerTopologySchema.h#L86
        // says that instanceLocations is only meaningful for native
        // instancing, empty for point instancing.
        auto instanceLocationsDs = instancerTopologySchema.GetInstanceLocations();
        if (!instanceLocationsDs) {
            auto primSelections = isolateSelection->GetPrimSelections(instancerPath);

            // If the instancer is in our list of collected instancers, it must
            // have prim selections.
            if (!TF_VERIFY(!primSelections.empty(), "Empty prim selections for instancer %s", instancerPath.GetText())) {
                continue;
            }

            std::set<int> visibleIndices;
            for (const auto& primSelection : primSelections) {
                for (const auto& instancesSelection : primSelection.nestedInstanceIndices) {
                    // When will instancesSelection.instanceIndices have more
                    // than one index?
                    for (auto instanceIndex : instancesSelection.instanceIndices) {
                        visibleIndices.insert(instanceIndex);
                    }
                }
            }

            auto nbInstances = getNbInstances(instancerTopologySchema);

            VtArray<bool> instanceMask(nbInstances);
            for (decltype(nbInstances) i=0; i < nbInstances; ++i) {
                instanceMask[i] = (visibleIndices.count(i) > 0);
            }

            instancerMasks[instancerPath] = instanceMask;
            continue;
        }

        auto instanceLocations = instanceLocationsDs->GetTypedValue(0.0f);

        VtArray<bool> instanceMask(instanceLocations.size());
        std::size_t i=0;
        for (const auto& instanceLocation : instanceLocations) {
            const bool included = isolateSelection->HasAncestorOrDescendantInclusive(instanceLocation);

            instanceMask[i] = included;
            ++i;
        }

        instancerMasks[instancerPath] = instanceMask;
    }

    return instancerMasks;
}

void IsolateSelectSceneIndex::_DirtyInstancerMasks(
    const InstancerMasks& newInstancerMasks
)
{
    // Keep paths in a set to minimize dirtying.
    std::set<SdfPath> dirtyPaths;

    // First clear old paths.
    for (const auto& entry : _instancerMasks) {
        dirtyPaths.insert(entry.first);
    }

    // Then add new paths.
    for (const auto& entry : newInstancerMasks) {
        dirtyPaths.insert(entry.first);
    }

    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;

    // Dirty all cleared and added prim paths.
    for (const auto& primPath : dirtyPaths) {
        _AddDirtyInstancerMaskEntry(primPath, &dirtiedEntries);
    }

    _SendPrimsDirtied(dirtiedEntries);
    
}

void IsolateSelectSceneIndex::_AddDirtyInstancerMaskEntry(
    const SdfPath&                            primPath, 
    HdSceneIndexObserver::DirtiedPrimEntries* dirtiedEntries
) const
{
    TF_DEBUG(FVP_ISOLATE_SELECT_SCENE_INDEX)
        .Msg("            %s: marking %s mask locator dirty.\n", _viewportId.c_str(), primPath.GetText());

    dirtiedEntries->emplace_back(primPath, instancerMaskLocator);
}

} //end of namespace FVP_NS_DEF
