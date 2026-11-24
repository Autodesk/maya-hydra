//
// Copyright 2019 Luma Pictures
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
        MAYAHYDRALIB_SCENE_INDEX, "Print info about the Maya Hydra scene index.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        MAYAHYDRALIB_RENDER_THREAD,
        "Print when the main thread waits more than 500 ms for the render "
        "thread to leave the render index, as the first sync of a heavy scene "
        "does.");

    TF_DEBUG_ENVIRONMENT_SYMBOL(
        MAYAHYDRALIB_RENDER_THREAD_CAPTURE,
        "Warn when a prim the render thread's capture copies serves a name its "
        "container does not list, which the copy drops. Probes every prim copied "
        "in full, so it slows the copy.");
}

PXR_NAMESPACE_CLOSE_SCOPE
