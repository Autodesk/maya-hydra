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
#include "debugCodes.h"

#include <pxr/base/tf/registryManager.h>

PXR_NAMESPACE_OPEN_SCOPE

// Some variables to enable debug printing information
TF_REGISTRY_FUNCTION(TfDebug)
{
    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_SELECTION_SCENE_INDEX,
        "Print information about the Flow Viewport selection scene index.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_SELECTION_TASK,
        "Print information about the Flow Viewport selection task.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_SELECTION_TRACKER,
        "Print information about the Flow Viewport selection tracker.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_APP_SELECTION_CHANGE,
        "Print information about application selection changes sent to the Flow Viewport.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_MERGING_SCENE_INDEX,
        "Print information about the Flow Viewport merging scene index.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX,
        "Print information about the Flow Viewport wireframe selection highlight scene index.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        PXR_NS::FVP_ISOLATE_SELECT_SCENE_INDEX,
        "Print information about the Flow Viewport isolate select scene index.");
}

PXR_NAMESPACE_CLOSE_SCOPE
