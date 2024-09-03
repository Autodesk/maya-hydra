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
#ifndef FVP_DEBUG_CODES_H
#define FVP_DEBUG_CODES_H

#include <pxr/base/tf/debug.h>
#include <pxr/pxr.h>

// No success in defining TF_DEBUG codes in anything but the pxr namespace.
// PPT, 29-Jun-2023.

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
TF_DEBUG_CODES(
   FVP_SELECTION_SCENE_INDEX
 , FVP_SELECTION_TASK
 , FVP_SELECTION_TRACKER
 , FVP_APP_SELECTION_CHANGE
 , FVP_MERGING_SCENE_INDEX
 , FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX
 , FVP_ISOLATE_SELECT_SCENE_INDEX
);
// clang-format on

PXR_NAMESPACE_CLOSE_SCOPE

#endif // FVP_DEBUG_CODES_H
