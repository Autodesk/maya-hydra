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
#ifndef MAYAHYDRALIB_PLUGIN_DEBUG_CODES_H
#define MAYAHYDRALIB_PLUGIN_DEBUG_CODES_H

#include <pxr/base/tf/debug.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

//! \brief  Some variables to enable debug printing information

// clang-format off
TF_DEBUG_CODES(
    MAYAHYDRALIB_RENDEROVERRIDE_DEFAULT_LIGHTING,
    MAYAHYDRALIB_RENDEROVERRIDE_RENDER,
    MAYAHYDRALIB_RENDEROVERRIDE_RESOURCES,
    MAYAHYDRALIB_RENDEROVERRIDE_SELECTION,
    MAYAHYDRALIB_RENDEROVERRIDE_SCENE_INDEX_CHAIN_MGMT
);
// clang-format on

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_PLUGIN_DEBUG_CODES_H
