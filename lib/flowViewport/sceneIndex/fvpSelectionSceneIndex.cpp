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

#include "flowViewport/sceneIndex/fvpSelectionSceneIndex.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"
#include "flowViewport/selection/fvpSelection.h"

#include "flowViewport/debugCodes.h"

#include "pxr/imaging/hd/retainedDataSource.h"
#include "pxr/imaging/hd/selectionSchema.h"
#include "pxr/imaging/hd/selectionsSchema.h"

#include <ufe/pathString.h>
#include <ufe/selection.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
const HdDataSourceLocatorSet selectionsSchemaDefaultLocator{HdSelectionsSchema::GetDefaultLocator()};
}

namespace FVP_NS_DEF {

class _PrimSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_PrimSource);

    _PrimSource(HdContainerDataSourceHandle const &inputSource,
                const Selection::ConstPtr& selection,
                const SdfPath &primPath)
        : _inputSource(inputSource)
        , _selection(selection)
        , _primPath(primPath)
    {
    }

    TfTokenVector GetNames() override
    {
        TfTokenVector names = _inputSource->GetNames();
        if (_selection->IsFullySelected(_primPath)) {
            names.push_back(HdSelectionsSchemaTokens->selections);
        }
        return names;
    }

    HdDataSourceBaseHandle Get(const TfToken &name) override
    {
        if (name == HdSelectionsSchemaTokens->selections) {
            return _selection->GetVectorDataSource(_primPath);
        }

        return _inputSource->Get(name);
    }

private:
    HdContainerDataSourceHandle const _inputSource;
    const Selection::ConstPtr _selection;
    const SdfPath _primPath;
};

SelectionSceneIndexRefPtr
SelectionSceneIndex::New(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const Selection::Ptr&         selection
)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::New() called.\n");
    return TfCreateRefPtr(new SelectionSceneIndex(inputSceneIndex, selection));
}

SelectionSceneIndex::
SelectionSceneIndex(
    const HdSceneIndexBaseRefPtr& inputSceneIndex,
    const Selection::Ptr&         selection
)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
  , _selection(selection)
  , _inputSceneIndexPathInterface(dynamic_cast<const PathInterface*>(&*_GetInputSceneIndex()))
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::SelectionSceneIndex() called.\n");

    TF_AXIOM(_inputSceneIndexPathInterface);
}

HdSceneIndexPrim
SelectionSceneIndex::GetPrim(const SdfPath &primPath) const
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::GetPrim() called.\n");

    HdSceneIndexPrim result = _GetInputSceneIndex()->GetPrim(primPath);
    if (!result.dataSource) {
        return result;
    }
    
    result.dataSource = _PrimSource::New(
        result.dataSource, _selection, primPath);

    return result;
}

SdfPathVector
SelectionSceneIndex::GetChildPrimPaths(const SdfPath &primPath) const
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::GetChildPrimPaths() called.\n");

    return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
SelectionSceneIndex::AddSelection(const Ufe::Path& appPath)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::AddSelection(const Ufe::Path& %s) called.\n", Ufe::PathString::string(appPath).c_str());

    // Call our input scene index to convert the application path to a scene
    // index path.
    auto sceneIndexPath = SceneIndexPath(appPath);

    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("    Adding %s to the Hydra selection.\n", sceneIndexPath.GetText());

    if (_selection->Add(sceneIndexPath)) {
        _SendPrimsDirtied({{sceneIndexPath, selectionsSchemaDefaultLocator}});
    }
}

void SelectionSceneIndex::RemoveSelection(const Ufe::Path& appPath)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::RemoveSelection(const Ufe::Path& %s) called.\n", Ufe::PathString::string(appPath).c_str());

    // Call our input scene index to convert the application path to a scene
    // index path.
    auto sceneIndexPath = SceneIndexPath(appPath);

    if (_selection->Remove(sceneIndexPath)) {
        _SendPrimsDirtied({{sceneIndexPath, selectionsSchemaDefaultLocator}});
    }
}

void
SelectionSceneIndex::ClearSelection()
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::ClearSelection() called.\n");

    if (_selection->IsEmpty()) {
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries entries;
    auto paths = _selection->GetFullySelectedPaths();
    entries.reserve(paths.size());
    for (const auto& path : paths) {
        entries.emplace_back(path, selectionsSchemaDefaultLocator);
    }

    _selection->Clear();

    _SendPrimsDirtied(entries);
}

void SelectionSceneIndex::ReplaceSelection(const Ufe::Selection& selection)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::ReplaceSelection() called.\n");

    // Process the selection replace by performing dirty notification of the
    // existing selection state.  We could do this more efficiently by
    // accounting for overlapping previous and new selections.
    HdSceneIndexObserver::DirtiedPrimEntries entries;
    auto paths = _selection->GetFullySelectedPaths();
    entries.reserve(paths.size() + selection.size());
    for (const auto& path : paths) {
        entries.emplace_back(path, selectionsSchemaDefaultLocator);
    }

    _selection->Clear();

    SdfPathVector sceneIndexSn;
    sceneIndexSn.reserve(selection.size());
    for (const auto& snItem : selection) {
        // Call our input scene index to convert the application path to a scene
        // index path.
        auto sceneIndexPath = SceneIndexPath(snItem->path());

        if (sceneIndexPath.IsEmpty()) {
            continue;
        }

        sceneIndexSn.emplace_back(sceneIndexPath);
        TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
            .Msg("    Adding %s to the Hydra selection.\n", sceneIndexPath.GetText());
        entries.emplace_back(sceneIndexPath, selectionsSchemaDefaultLocator);
    }

    _selection->Replace(sceneIndexSn);
    _SendPrimsDirtied(entries);
}

bool SelectionSceneIndex::IsFullySelected(const SdfPath& primPath) const
{
    return _selection->IsFullySelected(primPath);
}

bool SelectionSceneIndex::HasFullySelectedAncestorInclusive(const SdfPath& primPath) const
{
    return _selection->HasFullySelectedAncestorInclusive(primPath);
}

SdfPath SelectionSceneIndex::SceneIndexPath(const Ufe::Path& appPath) const
{
    auto sceneIndexPath = _inputSceneIndexPathInterface->SceneIndexPath(appPath);

    if (sceneIndexPath.IsEmpty()) {
        TF_WARN("SelectionSceneIndex::SceneIndexPath(%s) returned an empty path, Hydra selection will be incorrect", Ufe::PathString::string(appPath).c_str());
    }

    return sceneIndexPath;
}

SdfPathVector SelectionSceneIndex::GetFullySelectedPaths() const
{
    return _selection->GetFullySelectedPaths();
}

void
SelectionSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::_PrimsAdded() called.\n");

    _SendPrimsAdded(entries);
}

void
SelectionSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::_PrimsDirtied() called.\n");

    _SendPrimsDirtied(entries);
}

void
SelectionSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    TF_DEBUG(FVP_SELECTION_SCENE_INDEX)
        .Msg("SelectionSceneIndex::_PrimsRemoved() called.\n");

    if (!_selection->IsEmpty()) {
        for (const auto &entry : entries) {
            _selection->RemoveHierarchy(entry.primPath);
        }
    }

    _SendPrimsRemoved(entries);
}

}
