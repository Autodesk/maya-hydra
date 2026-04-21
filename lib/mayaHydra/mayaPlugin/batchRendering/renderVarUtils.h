//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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
#ifndef MAYAHYDRA_RENDER_VAR_UTILS_H
#define MAYAHYDRA_RENDER_VAR_UTILS_H

#include "batchRenderTypes.h"

#include <pxr/pxr.h>
#include <pxr/imaging/hd/types.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE
class UsdRenderProduct;
class TfToken;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

RenderVarsInfo GetRenderVarsFromUsdRenderProduct(
    const PXR_NS::UsdRenderProduct& renderProduct);

PXR_NS::HdFormat GetHdFormatFromRenderVarDataType(const PXR_NS::TfToken& dataType);

std::string SanitizeAovNameForFileName(const std::string& name);
std::string AppendAovSuffixToFileName(const std::string& fileName, const std::string& aovSuffix);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_VAR_UTILS_H
