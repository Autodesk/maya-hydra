//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRALIB_TOKENS_H
#define MAYAHYDRALIB_TOKENS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/tf/staticTokens.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define MAYAHYDRA_PICK_OPTIONVAR_TOKENS \
    ((GeomSubsetsPickMode, "mayaHydra_GeomSubsetsPickMode"))
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MayaHydraPickOptionVars, MAYAHYDRALIB_API, MAYAHYDRA_PICK_OPTIONVAR_TOKENS);

// clang-format off
#define GEOMSUBSETS_PICK_MODE_TOKENS \
    (None) \
    (Faces)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(GeomSubsetsPickModeTokens, MAYAHYDRALIB_API, GEOMSUBSETS_PICK_MODE_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_TOKENS_H
