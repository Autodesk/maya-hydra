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
#include "renderVarUtils.h"

#include "pluginDebugCodes.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/relationship.h>
#include <pxr/usd/usdRender/product.h>
#include <pxr/usd/usdRender/tokens.h>
#include <pxr/usd/usdRender/var.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <map>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

RenderVarsInfo GetRenderVarsFromUsdRenderProduct(
    const UsdRenderProduct& renderProduct)
{
    RenderVarsInfo renderVarsInfo;
    if (!renderProduct) {
        TF_WARN("GetRenderVarsFromUsdRenderProduct: Render product invalid; "
                "no render vars.\n");
        return renderVarsInfo;
    }

    UsdRelationship orderedVarsRel = renderProduct.GetOrderedVarsRel();
    SdfPathVector   varTargets;
    if (orderedVarsRel.GetTargets(&varTargets)) {
        for (const SdfPath& varPath : varTargets) {
            UsdPrim varPrim = renderProduct.GetPrim().GetStage()->GetPrimAtPath(varPath);
            if (!varPrim.IsValid()) {
                TF_WARN(
                    "GetRenderVarsFromUsdRenderProduct: Render var prim invalid (%s) "
                    "on product %s\n",
                    varPath.GetText(),
                    renderProduct.GetPrim().GetPath().GetText());
                continue;
            }

            UsdRenderVar renderVar(varPrim);
            if (!renderVar) {
                TF_WARN(
                    "GetRenderVarsFromUsdRenderProduct: Render var schema invalid (%s) "
                    "on product %s\n",
                    varPath.GetText(),
                    renderProduct.GetPrim().GetPath().GetText());
                continue;
            }

            TfToken varName(varPrim.GetName());

            TfToken     sourceType;
            std::string sourceName;
            renderVar.GetSourceTypeAttr().Get(&sourceType);
            renderVar.GetSourceNameAttr().Get(&sourceName);

            if (varName.IsEmpty() && !sourceName.empty()) {
                varName = TfToken(sourceName);
            }

            if (sourceType == UsdRenderTokens->lpe) {
                std::string lpeExpr = !sourceName.empty() ? sourceName : varName.GetString();
                if (!lpeExpr.empty()) {
                    varName = TfToken(std::string("lpe:") + lpeExpr);
                }
            } else if (!sourceName.empty()) {
                if (sourceName == "Ci") {
                    varName = HdAovTokens->color;
                } else if (sourceName == "z") {
                    varName = HdAovTokens->depth;
                }
            }

            TfToken dataType;
            renderVar.GetDataTypeAttr().Get(&dataType);

            if (!varName.IsEmpty()
                && std::find(
                       renderVarsInfo.renderVars.begin(),
                       renderVarsInfo.renderVars.end(),
                       varName) == renderVarsInfo.renderVars.end()) {
                renderVarsInfo.renderVars.push_back(varName);
                if (!dataType.IsEmpty()) {
                    renderVarsInfo.dataTypes[varName] = dataType;
                }
                TF_DEBUG_MSG(
                    MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
                    "Render var added (%s): %s\n",
                    varPath.GetText(),
                    varName.GetText());
            }
        }
    } else {
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No ordered render vars on product %s\n",
            renderProduct.GetPrim().GetPath().GetText());
    }

    if (renderVarsInfo.renderVars.empty()) {
        renderVarsInfo.renderVars.push_back(HdAovTokens->color);
        TF_DEBUG_MSG(
            MAYAHYDRAPLUGIN_BATCHRENDER_CMD,
            "No render vars found on product %s; defaulting to %s\n",
            renderProduct.GetPrim().GetPath().GetText(),
            HdAovTokens->color.GetText());
    }

    return renderVarsInfo;
}

HdFormat GetHdFormatFromRenderVarDataType(const TfToken& dataType)
{
    if (dataType.IsEmpty()) {
        return HdFormatInvalid;
    }

    static const std::map<std::string, HdFormat> kDataTypeToFormat = {
        {"float", HdFormatFloat32},
        {"half", HdFormatFloat16},
        {"double", HdFormatFloat32},
        {"int", HdFormatInt32},
        {"float2", HdFormatFloat32Vec2},
        {"float3", HdFormatFloat32Vec3},
        {"color3f", HdFormatFloat32Vec3},
        {"normal3f", HdFormatFloat32Vec3},
        {"point3f", HdFormatFloat32Vec3},
        {"vector3f", HdFormatFloat32Vec3},
        {"float4", HdFormatFloat32Vec4},
        {"color4f", HdFormatFloat32Vec4},
        {"half2", HdFormatFloat16Vec2},
        {"half3", HdFormatFloat16Vec3},
        {"half4", HdFormatFloat16Vec4},
        {"int2", HdFormatInt32Vec2},
        {"int3", HdFormatInt32Vec3},
        {"int4", HdFormatInt32Vec4},
        {"double2", HdFormatFloat32Vec2},
        {"double3", HdFormatFloat32Vec3},
        {"double4", HdFormatFloat32Vec4},
    };

    const std::string type = dataType.GetString();
    const auto        it = kDataTypeToFormat.find(type);
    return it != kDataTypeToFormat.end() ? it->second : HdFormatInvalid;
}

std::string SanitizeAovNameForFileName(const std::string& name)
{
    std::string sanitized = name;
    for (char& c : sanitized) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_' && c != '-') {
            c = '_';
        }
    }
    if (sanitized.empty()) {
        sanitized = "aov";
    }
    return sanitized;
}

std::string AppendAovSuffixToFileName(const std::string& fileName, const std::string& aovSuffix)
{
    if (aovSuffix.empty()) {
        return fileName;
    }
    std::filesystem::path path(fileName);
    const std::string stem = path.stem().string();
    const std::string ext = path.extension().string();
    path.replace_filename(stem + "_" + aovSuffix + ext);
    return path.string();
}

} // namespace MAYAHYDRA_NS_DEF
