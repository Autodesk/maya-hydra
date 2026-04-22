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

#ifndef FLOW_VIEWPORT_API_RENDERVIEWDATA_ISOLATE_SELECT_MANAGER_H
#define FLOW_VIEWPORT_API_RENDERVIEWDATA_ISOLATE_SELECT_MANAGER_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/selection/fvpSelectionTypes.h"

#include <pxr/usd/sdf/path.h>

#include <map>
#include <string>
#include <unordered_set>

namespace FVP_NS_DEF {

/// \class IsolateSelectManager
///
/// Manages the shared isolate selection state.
/// Provides access to a shared isolate select scene index and per-viewport isolate selections.
///
class FVP_API IsolateSelectManager
{
public:
    static IsolateSelectManager& Get();

    SelectionPtr GetOrCreateIsolateSelection(const std::string& viewportId);
    SelectionPtr GetIsolateSelection(const std::string& viewportId) const;
    void DisableIsolateSelection(const std::string& viewportId);

    void AddIsolateSelection(
        const std::string& viewportId,
        const PrimSelections& primSelections);
    void RemoveIsolateSelection(
        const std::string& viewportId,
        const PrimSelections& primSelections);
    void ReplaceIsolateSelection(
        const std::string& viewportId,
        const SelectionPtr& selection);
    void ClearIsolateSelection(const std::string& viewportId);

    // Per-viewport force-visible path set.  These are paths whose visibility
    // must be forced ON when included in isolate select (typically the
    // Maya-native gizmo render items for selected USD cameras and lights).
    // The manager keeps a map keyed by viewportId and pushes the active
    // viewport's set to the shared isolate select scene index whenever the
    // viewport changes, so a callback from one panel cannot overwrite or
    // clear another panel's force-visible paths.
    void SetForceVisiblePaths(
        const std::string&                                                viewportId,
        std::unordered_set<PXR_NS::SdfPath, PXR_NS::SdfPath::Hash>&&      paths);
    void ClearForceVisiblePaths(const std::string& viewportId);

    // Get and set the isolate select scene index.  This scene index provides
    // isolate select services for all viewports.
    IsolateSelectSceneIndexRefPtr GetIsolateSelectSceneIndex() const;
    void SetIsolateSelectSceneIndex(const IsolateSelectSceneIndexRefPtr& sceneIndex);

    /// Clear all isolate select state.
    void Reset();

private:
    IsolateSelectManager() = default;
    IsolateSelectManager(const IsolateSelectManager&) = delete;
    IsolateSelectManager& operator=(const IsolateSelectManager&) = delete;

    SelectionPtr _EnableIsolateSelection(const std::string& viewportId);
    void _EnableIsolateSelectAndSetViewport(const std::string& viewportId);

    // Push the viewportId's force-visible paths (or an empty set if none) to
    // the shared scene index, so its active set always matches the active
    // viewport.  Must be called after any operation that switches the scene
    // index's active viewport.
    void _PushForceVisiblePathsToSceneIndex(const std::string& viewportId);

    // Isolate selection, keyed by viewportId.  A null selection pointer means
    // isolate select for that viewport is disabled.  Disabling isolate select
    // on a viewport clears its isolate selection, so that at next isolate
    // select enable for that viewport its isolate selection is empty.
    std::map<std::string, SelectionPtr> _isolateSelection;

    // Force-visible paths keyed by viewportId.  An entry is removed when
    // isolate select is disabled for the viewport (DisableIsolateSelection)
    // or explicitly via ClearForceVisiblePaths().
    std::map<std::string, std::unordered_set<PXR_NS::SdfPath, PXR_NS::SdfPath::Hash>>
        _forceVisiblePaths;

    // Isolate select scene index.
    IsolateSelectSceneIndexRefPtr _isolateSelectSceneIndex;
};

} // namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_RENDERVIEWDATA_ISOLATE_SELECT_MANAGER_H
