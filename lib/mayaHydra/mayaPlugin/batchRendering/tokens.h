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
// Copyright 2026 Autodesk
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
#ifndef BATCH_RENDER_TOKENS_H
#define BATCH_RENDER_TOKENS_H

#include <pxr/base/tf/staticTokens.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define BATCH_RENDER_TOKENS \
    (renderSettingsSrc) \
    (settingsMapRenderSettingsSrc) \
    (hydraSceneRenderSettingsSrc) \
    (adsk_ocioPath)  // Autodesk-specific OCIO config path (no standard Hydra token).
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(BatchRenderTokens, , BATCH_RENDER_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // BATCH_RENDER_TOKENS_H
