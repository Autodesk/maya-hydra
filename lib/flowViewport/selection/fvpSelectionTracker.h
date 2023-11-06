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
#ifndef FVP_SELECTION_TRACKER_H
#define FVP_SELECTION_TRACKER_H

#include "flowViewport/api.h"

#include <pxr/pxr.h>
#include <pxr/imaging/hd/selection.h> // For HdSelectionSharedPtr typedef

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class HdRenderIndex;

PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

using SelectionTrackerSharedPtr =
    std::shared_ptr<class SelectionTracker>;

/// Placeholder for
///
/// https://github.com/PixarAnimationStudios/OpenUSD/blob/release/pxr/imaging/hdx/selectionTracker.h
///
/// which is Hydra Storm-centric.  To be revised.  PPT, 27-Sep-2023.

/// \class SelectionTracker
///
/// The selection tracker owns the HdSelection and the selection scene index
/// observer that maintains the selection up to date.  
/// 
/// HdxSelectionTask takes SelectionTracker as a task parameter, to inject
/// the selection into the list of tasks.

class SelectionTracker
{
public:
    FVP_API
    SelectionTracker();
    FVP_API
    virtual ~SelectionTracker();

    /// Returns a monotonically increasing version number, which increments
    /// whenever the selection has changed. Note that this number may
    /// overflow and become negative, thus clients should use a not-equal
    /// comparison.
    FVP_API
    int GetVersion() const;

    FVP_API
    PXR_NS::HdSelectionSharedPtr GetSelection(const PXR_NS::HdRenderIndex *index) const;

private:

    // A helper class to obtain the selection computed by querying the scene
    // indices (with the HdxSelectionSceneIndexObserver).
    class _Selection;
    std::unique_ptr<_Selection> _selection;
};

}

#endif //FVP_SELECTION_TRACKER_H
