//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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


/// Is the definition of a customer Hydra client to register a set of callbacks for a Hydra viewport.
#ifndef FLOW_VIEWPORT_API_VIEWPORT_API_TOKENS_H
#define FLOW_VIEWPORT_API_VIEWPORT_API_TOKENS_H

#include "flowViewport/api.h"

#include <pxr/base/tf/staticTokens.h>

// *** TODO / FIXME ***  Figure out how to put tokens into non-Pixar namespace.

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define FVP_VIEWPORT_API_TOKENS\
    /** Use this string in the viewport identifier parameters, named "hydraViewportId" in this class, to apply the data producer scene index to all viewports.*/\
    (allViewports) \
    /**  Use this string for the "rendererNames" parameter to apply to all renderers.*/\
    (allRenderers)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(FvpViewportAPITokens, FVP_API, FVP_VIEWPORT_API_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_API_VIEWPORT_API_TOKENS_H
