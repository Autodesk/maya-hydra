//
// Copyright 2025 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#include "fvpIsolateSelectManager.h"

#include "flowViewport/selection/fvpSelection.h"

#include <pxr/base/tf/diagnostic.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

IsolateSelectManager&
IsolateSelectManager::Get()
{
    static IsolateSelectManager instance;
    return instance;
}

SelectionPtr
IsolateSelectManager::GetOrCreateIsolateSelection(const std::string& viewportId)
{
    auto found = _isolateSelection.find(viewportId);
    if (found != _isolateSelection.end()) {
        return found->second;
    }
    // Initially isolate selection is disabled.
    _isolateSelection[viewportId] = nullptr;
    return nullptr;
}

SelectionPtr
IsolateSelectManager::GetIsolateSelection(const std::string& viewportId) const
{
    auto found = _isolateSelection.find(viewportId);
    return (found != _isolateSelection.end()) ? found->second : nullptr;
}

void
IsolateSelectManager::DisableIsolateSelection(const std::string& viewportId)
{
    _isolateSelection[viewportId] = nullptr;
    // Disabling isolate select on a viewport also clears its force-visible
    // paths so that re-enabling starts from a clean state.
    _forceVisiblePaths.erase(viewportId);
    _isolateSelectSceneIndex->SetViewport(viewportId, nullptr);
    _PushForceVisiblePathsToSceneIndex(viewportId);
}

SelectionPtr
IsolateSelectManager::_EnableIsolateSelection(const std::string& viewportId)
{
    auto& selection = _isolateSelection.at(viewportId);

    // If the viewport didn't have an isolate selection because it was
    // disabled, create it now.
    if (!selection) {
        selection = std::make_shared<Selection>();
        _isolateSelection[viewportId] = selection;
    }
    return selection;
}

void
IsolateSelectManager::AddIsolateSelection(
    const std::string&    viewportId,
    const PrimSelections& primSelections)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->AddIsolateSelection(primSelections);
}

void
IsolateSelectManager::RemoveIsolateSelection(
    const std::string&    viewportId,
    const PrimSelections& primSelections)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->RemoveIsolateSelection(primSelections);
}

void
IsolateSelectManager::ReplaceIsolateSelection(
    const std::string&  viewportId,
    const SelectionPtr& isolateSelection)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _isolateSelection[viewportId] = isolateSelection;
    _isolateSelectSceneIndex->SetViewport(viewportId, isolateSelection);
    _PushForceVisiblePathsToSceneIndex(viewportId);
}

void
IsolateSelectManager::SetForceVisiblePaths(
    const std::string&     viewportId,
    TfHashSet<SdfPath, SdfPath::Hash>&& paths)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _forceVisiblePaths[viewportId] = std::move(paths);
    // If this viewport is the active one on the shared scene index, push the
    // updated set immediately.  Otherwise the set will be pushed when the
    // viewport becomes active (via ReplaceIsolateSelection /
    // _EnableIsolateSelectAndSetViewport).
    if (_isolateSelectSceneIndex->GetViewportId() == viewportId) {
        _PushForceVisiblePathsToSceneIndex(viewportId);
    }
}

void
IsolateSelectManager::ClearForceVisiblePaths(const std::string& viewportId)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _forceVisiblePaths.erase(viewportId);
    if (_isolateSelectSceneIndex->GetViewportId() == viewportId) {
        _isolateSelectSceneIndex->ClearForceVisiblePaths();
    }
}

void
IsolateSelectManager::ClearIsolateSelection(const std::string& viewportId)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->ClearIsolateSelection();
}

void
IsolateSelectManager::SetIsolateSelectSceneIndex(
    const IsolateSelectSceneIndexRefPtr& sceneIndex)
{
    _isolateSelectSceneIndex = sceneIndex;
    // If we're resetting the isolate select scene index, we're starting anew,
    // so clear out existing isolate selections.
    _isolateSelection.clear();
    _forceVisiblePaths.clear();
}

IsolateSelectSceneIndexRefPtr
IsolateSelectManager::GetIsolateSelectSceneIndex() const
{
    return _isolateSelectSceneIndex;
}

void
IsolateSelectManager::_EnableIsolateSelectAndSetViewport(
    const std::string& viewportId)
{
    const bool enabled{_isolateSelectSceneIndex->GetIsolateSelection()};
    auto isolateSelection = _EnableIsolateSelection(viewportId);

    // If the isolate select scene index is not set to the right viewport,
    // do a viewport switch.
    if (_isolateSelectSceneIndex->GetViewportId() != viewportId) {
        _isolateSelectSceneIndex->SetViewport(viewportId, isolateSelection);
        _PushForceVisiblePathsToSceneIndex(viewportId);
    }
    else if (!enabled) {
        // Same viewport, so no viewport switch, but must move from disabled to
        // enabled for that viewport.
        _isolateSelectSceneIndex->SetIsolateSelection(isolateSelection);
        _PushForceVisiblePathsToSceneIndex(viewportId);
    }
}

void
IsolateSelectManager::_PushForceVisiblePathsToSceneIndex(
    const std::string& viewportId)
{
    auto found = _forceVisiblePaths.find(viewportId);
    if (found == _forceVisiblePaths.end()) {
        _isolateSelectSceneIndex->ClearForceVisiblePaths();
        return;
    }
    // Copy because the scene index moves the set in.  We must keep our
    // per-viewport copy so we can re-push it after a future viewport switch.
    auto copy = found->second;
    _isolateSelectSceneIndex->SetForceVisiblePaths(std::move(copy));
}

void
IsolateSelectManager::Reset()
{
    _isolateSelection.clear();
    _forceVisiblePaths.clear();
    _isolateSelectSceneIndex = nullptr;
}

} // namespace FVP_NS_DEF

