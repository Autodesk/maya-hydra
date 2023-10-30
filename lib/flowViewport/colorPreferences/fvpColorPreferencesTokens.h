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
#ifndef FVP_COLOR_PREFERENCES_TOKENS_H
#define FVP_COLOR_PREFERENCES_TOKENS_H

#include "flowViewport/api.h"

#include <pxr/base/tf/staticTokens.h>

// *** TODO / FIXME ***  Figure out how to put tokens into non-Pixar namespace.

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define FVP_COLOR_PREFERENCES_TOKENS \
    /* The "unspecified" token should only be used by hosts who are incapable of sending precise notifications. */ \
    /* In such cases, it should be used only when sending notifications, and the colors contained in the */ \
    /* corresponding ColorChanged notification should be ignored by the notification recipient. */ \
    (unspecified) \
    (wireframeSelection) \
    (wireframeSelectionSecondary) \
    (vertexSelection) \
    (edgeSelection) \
    (faceSelection)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(FvpColorPreferencesTokens, FVP_API, FVP_COLOR_PREFERENCES_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // FVP_COLOR_PREFERENCES_TOKENS_H
