//
// Copyright 2016 Pixar
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

/// Placeholder for
///
/// https://github.com/PixarAnimationStudios/OpenUSD/blob/release/pxr/imaging/hdx/selectionTracker.cpp
///
/// which is Hydra Storm-centric.  To be revised.  PPT, 27-Sep-2023.

#include "flowViewport/selection/fvpSelectionTracker.h"

#include "flowViewport/debugCodes.h"

#include <pxr/imaging/hdx/selectionSceneIndexObserver.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/selection.h>
#include <pxr/base/tf/debug.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

class SelectionTracker::_Selection
{
public:
    // Selection task has initial selection version at -1, so match that.
    _Selection() : _lastVersion(-1) {}

    // Returns the selection from the scene index.
    HdSelectionSharedPtr
    GetSelection(const HdRenderIndex * const index)
    {
        // Tell scene index observer what scene index to observe.
        // FLOW_VIEWPORT_TODO  Why are we setting the scene index onto the
        // observer at every call?  PPT, 27-Sep-2023.
        _observer.SetSceneIndex(index->GetTerminalSceneIndex());

        // Recompute if changed since last time it was computed.
        if (_lastVersion != GetVersion()) {
            _selection = _observer.GetSelection();
            _lastVersion = GetVersion();
        }
        return _selection;
    }

    // Version number for the selection.
    int GetVersion() const {
        return _observer.GetVersion();
    }

private:
    // Cache the selection. The version of the selection cached here
    // is stored as _lastVersion.
    HdSelectionSharedPtr _selection;
    int _lastVersion;

    HdxSelectionSceneIndexObserver _observer;
};

SelectionTracker::SelectionTracker()
  : _selection(std::make_unique<_Selection>())
{}

SelectionTracker::~SelectionTracker() = default;

int
SelectionTracker::GetVersion() const
{
    return _selection->GetVersion();
}

HdSelectionSharedPtr SelectionTracker::GetSelection(const HdRenderIndex* index) const
{
    TF_DEBUG(FVP_SELECTION_TRACKER)
        .Msg("SelectionTracker::GetSelection() called.\n");

    return _selection->GetSelection(index);
}

}
