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
#include "flowViewport/selection/fvpSelectionTask.h"
#include "flowViewport/selection/fvpSelectionTracker.h"

#include "flowViewport/debugCodes.h"
#include "flowViewport/tokens.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

// -------------------------------------------------------------------------- //

SelectionTask::SelectionTask()
    : HdTask(Id())
    , _lastVersion(-1)
{
    TF_DEBUG(FVP_SELECTION_TASK)
        .Msg("SelectionTask::SelectionTask() called.\n");
}

SelectionTask::~SelectionTask() = default;

void
SelectionTask::Sync(HdSceneDelegate* ,
                    HdTaskContext* ctx,
                    HdDirtyBits* dirtyBits)
{
    TF_DEBUG(FVP_SELECTION_TASK)
        .Msg("SelectionTask::Sync() called.\n");

    HD_TRACE_FUNCTION();

    if ((*dirtyBits) & HdChangeTracker::DirtyParams) {
        // We track the version of selection tracker in the task to see if we
        // need need to update.  We don't have access to the selection tracker
        // (as it is in the task context) so we reset the version to -1 to
        // cause a version mismatch and force the update.
        _lastVersion = -1;
    }

    *dirtyBits = HdChangeTracker::Clean;
}

void
SelectionTask::Prepare(
    HdTaskContext* ctx,
    HdRenderIndex* renderIndex
)
{
    TF_DEBUG(FVP_SELECTION_TASK)
        .Msg("SelectionTask::Prepare() called.\n");

    Fvp::SelectionTrackerSharedPtr snTracker;
    if (!_GetTaskContextData(ctx, FvpTokens->fvpSelectionState, &snTracker)) {
        return;
    }

    auto trackerVersion = snTracker->GetVersion();

    if (trackerVersion == _lastVersion) {
        return;
    }

    _lastVersion = trackerVersion;

    // FLOW_VIEWPORT_TODO  Get the selection from the selection tracker here,
    // compute selection-derived buffers here, and place them back in the
    // context for downstream tasks to use.  PPT, 27-Sep-2023.
}

void
SelectionTask::Execute(HdTaskContext* ctx)
{
    TF_DEBUG(FVP_SELECTION_TASK)
        .Msg("SelectionTask::Execute() called.\n");

    HD_TRACE_FUNCTION();
    HF_MALLOC_TAG_FUNCTION();

    // Note that selectionTask comes after renderTask.
}

}
