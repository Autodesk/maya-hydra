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
#ifndef MAYAHYDRA_BATCH_RENDER_TYPES_H
#define MAYAHYDRA_BATCH_RENDER_TYPES_H

#include <mayaHydraLib/mayaHydra.h>

#include <pxr/base/tf/token.h>
#include <pxr/pxr.h>

#include <unordered_map>

namespace MAYAHYDRA_NS_DEF {

using RenderVarDataTypes = std::unordered_map<
    PXR_NS::TfToken,
    PXR_NS::TfToken,
    PXR_NS::TfToken::HashFunctor>;

struct RenderVarsInfo {
    PXR_NS::TfTokenVector renderVars;
    RenderVarDataTypes   dataTypes;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_BATCH_RENDER_TYPES_H
