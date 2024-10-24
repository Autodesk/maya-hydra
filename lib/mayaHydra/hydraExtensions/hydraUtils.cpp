//
// Copyright 2019 Luma Pictures
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

#include "hydraUtils.h"

#include <pxr/base/gf/quath.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/array.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/usd/sdf/assetPath.h>

#include <regex>

PXR_NAMESPACE_USING_DIRECTIVE

// This is the delimiter that Maya uses to identify levels of hierarchy in the
// Maya DAG.
constexpr char MayaDagDelimiter[] = "|";

// This is the delimiter that Maya uses to separate levels of namespace in
// Maya node names.
constexpr char MayaNamespaceDelimiter[] = ":";

namespace MAYAHYDRA_NS_DEF {

std::string ConvertVtValueToString(const VtValue& val)
{
    if (val.IsEmpty()) {
        return "No Value!";
    }

    std::ostringstream ss;
    if (val.IsHolding<bool>()) {
        const bool v = val.UncheckedGet<bool>();
        ss << "bool : ";
        ss << v;
    } else if (val.IsHolding<TfToken>()) {
        const TfToken elem = val.UncheckedGet<TfToken>();
        ss << "TfToken : " << elem.GetText();
    } else if (val.IsHolding<VtArray<int>>()) {
        auto arrayType = val.UncheckedGet<VtArray<int>>();
        ss << "VtArray<int> : (";
        for (auto elem : arrayType) {
            ss << std::to_string(elem) << " , ";
        }
        ss << ")";
    } else if (val.IsHolding<VtArray<float>>()) {
        auto arrayType = val.UncheckedGet<VtArray<float>>();
        ss << "VtArray<float> : (";
        for (auto elem : arrayType) {
            ss << std::to_string(elem) << " , ";
        }
        ss << ")";
    } else if (val.IsHolding<float>()) {
        const float v = val.UncheckedGet<float>();
        ss << "float : ";
        ss << v;
    } else if (val.IsHolding<int>()) {
        const int v = val.UncheckedGet<int>();
        ss << "int : ";
        ss << v;
    } else if (val.IsHolding<GfVec2f>()) {
        const GfVec2f v = val.UncheckedGet<GfVec2f>();
        ss << "GfVec2f : (" << v[0] << " , " << v[1] << ")";
    } else if (val.IsHolding<GfVec3f>()) {
        const GfVec3f v = val.UncheckedGet<GfVec3f>();
        ss << "GfVec3f : (" << v[0] << " , " << v[1] << " , " << v[2] << ")";
    } else if (val.IsHolding<GfVec3d>()) {
        const auto v = val.UncheckedGet<GfVec3d>();
        ss << "GfVec3d : (" << v[0] << " , " << v[1] << " , " << v[2] << ")";
    } else if (val.IsHolding<SdfAssetPath>()) {
        const SdfAssetPath v = val.UncheckedGet<SdfAssetPath>();
        const std::string  assetPath = v.GetAssetPath();
        ss << "SdfAssetPath : \"" << assetPath << "\"";
    } else if (val.IsHolding<VtArray<SdfPath>>()) {
        auto arrayType = val.UncheckedGet<VtArray<SdfPath>>();
        ss << "VtArray<SdfPath> : (";
        for (auto elem : arrayType) {
            ss << elem.GetText() << " , ";
        }
        ss << ")";
    } else if (val.IsHolding<VtArray<GfVec3f>>()) {
        auto arrayType = val.UncheckedGet<VtArray<GfVec3f>>();
        ss << "VtArray<GfVec3f> : (";
        for (auto elem : arrayType) {
            auto strVec3f = "(" + std::to_string(elem[0]) + ", " + std::to_string(elem[1]) + ", "
                + std::to_string(elem[2]) + ")";
            ss << strVec3f + " , ";
        }
        ss << ")";
    } else if (val.IsHolding<VtArray<GfVec3d>>()) {
        auto arrayType = val.UncheckedGet<VtArray<GfVec3d>>();
        ss << "VtArray<GfVec3d> : (";
        for (auto elem : arrayType) {
            auto strVec3f = "(" + std::to_string(elem[0]) + ", " + std::to_string(elem[1]) + ", "
                + std::to_string(elem[2]) + ")";
            ss << strVec3f + " , ";
        }
        ss << ")";
    } else if (val.IsHolding<VtArray<GfQuath>>()) {
        auto arrayType = val.UncheckedGet<VtArray<GfQuath>>();
        ss << "VtArray<GfQuath> : (";
        for (auto elem : arrayType) {
            auto quathh = "(" + std::to_string(elem.GetReal()) + ", "
                + std::to_string(elem.GetImaginary()[0]) + ", "
                + std::to_string(elem.GetImaginary()[1]) + ", "
                + std::to_string(elem.GetImaginary()[2]) + ")";
            ss << quathh + " , ";
        }
        ss << ")";
    } else if (val.IsHolding<GfQuath>()) {
        auto elem = val.UncheckedGet<GfQuath>();
        auto quathh = "(" + std::to_string(elem.GetReal()) + ", "
            + std::to_string(elem.GetImaginary()[0]) + ", " + std::to_string(elem.GetImaginary()[1])
            + ", " + std::to_string(elem.GetImaginary()[2]) + ")";
        ss << "GfQuath : " << quathh;
    } else if (val.IsHolding<GfMatrix4d>()) {
        auto mat4d = val.UncheckedGet<GfMatrix4d>();

        double data[4][4];
        mat4d.Get(data);
        auto strMat4d = std::string("(") + "{" + std::to_string(data[0][0]) + ", "
            + std::to_string(data[0][1]) + ", " + std::to_string(data[0][2]) + ", "
            + std::to_string(data[0][3]) + "}, " + "{" + std::to_string(data[1][0]) + ", "
            + std::to_string(data[1][1]) + ", " + std::to_string(data[1][2]) + ", "
            + std::to_string(data[1][3]) + "}, " + "{" + std::to_string(data[2][0]) + ", "
            + std::to_string(data[2][1]) + ", " + std::to_string(data[2][2]) + ", "
            + std::to_string(data[2][3]) + "}, " + "{" + std::to_string(data[3][0]) + ", "
            + std::to_string(data[3][1]) + ", " + std::to_string(data[3][2]) + ", "
            + std::to_string(data[3][3]) + "}" + ")";
        ss << "GfMatrix4d : " << strMat4d;
    }

    std::string valueString = ss.str();
    if (valueString.size() > 0) {
        return valueString;
    }

    // Unknown
    return "* Unknown Type *";
}

std::string StripNamespaces(const std::string& nodeName, const int nsDepth)
{
    if (nodeName.empty() || nsDepth == 0) {
        return nodeName;
    }

    std::stringstream ss;

    const std::vector<std::string> nodeNameParts
        = TfStringSplit(nodeName, MayaDagDelimiter);

    const bool isAbsolute = TfStringStartsWith(nodeName, MayaDagDelimiter);

    for (size_t i = 0u; i < nodeNameParts.size(); ++i) {
        if (i == 0u && isAbsolute) {
            // If nodeName was absolute, the first element in nodeNameParts
            // will be empty, so just skip it. The output path will be made
            // absolute with the next iteration.
            continue;
        }

        if (i != 0u) {
            ss << MayaDagDelimiter;
        }

        const std::vector<std::string> nsNameParts
            = TfStringSplit(nodeNameParts[i], MayaNamespaceDelimiter);

        const size_t nodeNameIndex = nsNameParts.size() - 1u;

        auto startIter = nsNameParts.begin();
        if (nsDepth < 0) {
            // If nsDepth is negative, we don't keep any namespaces, so advance
            // startIter to the last element in the vector, which is just the
            // node name.
            startIter += nodeNameIndex;
        } else {
            // Otherwise we strip as many namespaces as possible up to nsDepth,
            // but no more than what would leave us with just the node name.
            startIter += std::min(static_cast<size_t>(nsDepth), nodeNameIndex);
        }

        ss << TfStringJoin(startIter, nsNameParts.end(), MayaNamespaceDelimiter);
    }

    return ss.str();
}

// Elements of the path will be sanitized such that it is a valid SdfPath.
// This means it will replace Maya's namespace delimiter (':') with
// underscores ('_').
// A SdfPath in Pixar USD is considered invalid if it does not conform to the rules for path names.
// Some common issues that can make a path invalid include: Starting with a number : Path names
// must start with a letter, not a number. Including spaces or special characters : Path names can
// only contain letters, numbers, and the characters _, -, and : .
void SanitizeNameForSdfPath(std::string& inoutPathString, bool doStripNamespaces /*= false*/)
{
    if (doStripNamespaces) {
        // Drop namespaces instead of making them part of the path.
        inoutPathString = StripNamespaces(inoutPathString);
    }

    std::replace(
        inoutPathString.begin(),
        inoutPathString.end(),
        MayaDagDelimiter[0],
        SdfPathTokens->childDelimiter.GetString()[0]);
    std::replace(inoutPathString.begin(), inoutPathString.end(), MayaNamespaceDelimiter[0], '_');
    std::replace(inoutPathString.begin(), inoutPathString.end(), ',', '_');
    std::replace(inoutPathString.begin(), inoutPathString.end(), ';', '_');
}

std::string SanitizeNameForSdfPath(
    const std::string& pathString, 
    bool doStripNamespaces /* = false */
)
{
    auto rhs = pathString;
    SanitizeNameForSdfPath(rhs, doStripNamespaces);
    return rhs;
}

SdfPath MakeRelativeToParentPath(const SdfPath& path)
{
    return path.MakeRelativePath(path.GetParentPath());
}

bool GetXformMatrixFromPrim(const HdSceneIndexPrim& prim, GfMatrix4d& outMatrix)
{
    HdContainerDataSourceHandle xformContainer
        = HdContainerDataSource::Cast(prim.dataSource->Get(HdXformSchemaTokens->xform));
    if (!xformContainer) {
        return false;
    }
    HdXformSchema xform = HdXformSchema(xformContainer);
    if (!xform.GetMatrix()) {
        return false;
    }
    outMatrix = xform.GetMatrix()->GetValue(0).Get<GfMatrix4d>();
    return true;
}

void GetDirectionalLightPositionFromDirectionVector(GfVec3f& outPosition, const GfVec3f& direction)
{
    //To simulate a directional light which has no actual position, but doesn't seem to be supported in hydra, we set a position very very far
    //so it looks like a directional light.    
    constexpr float farfarAway {1.0e15f};//we use a point on the Z axis far far away
    outPosition = {-farfarAway*direction.data()[0], -farfarAway*direction.data()[1], -farfarAway*direction.data()[2]};
}

bool splitNumericalSuffix(const std::string& srcName, std::string& base, std::string& suffix)
{
    // Compiled regular expression to find a numerical suffix to a path component.
    // It searches for any number of characters followed by a single non-numeric,
    // then one or more digits at end of string.
    const static std::regex re("(.*)([^0-9])([0-9]+)$");
    base = srcName;
    std::smatch match;
    if (std::regex_match(srcName, match, re)) {
        base = match[1].str() + match[2].str();
        suffix = match[3].str();
        return true;
    }
    return false;
}

std::string uniqueName(const TfToken::HashSet& existingNames, std::string srcName)
{
    if (existingNames.empty() || (existingNames.count(TfToken(srcName)) == 0u)) {
        return srcName;
    }

    std::string base, suffixStr;
    int         suffix { 1 };
    size_t      lenSuffix { 1 };
    if (splitNumericalSuffix(srcName, base, suffixStr)) {
        lenSuffix = suffixStr.length();
        suffix = std::stoi(suffixStr) + 1;
    }

    // Create a suffix string from the number keeping the same number of digits
    // as numerical suffix from input srcName (padding with 0's if needed).
    suffixStr = std::to_string(suffix);
    suffixStr = std::string(lenSuffix - std::min(lenSuffix, suffixStr.length()), '0') + suffixStr;
    std::string dstName = base + suffixStr;
    while (existingNames.count(TfToken(dstName)) > 0) {
        suffixStr = std::to_string(++suffix);
        suffixStr
            = std::string(lenSuffix - std::min(lenSuffix, suffixStr.length()), '0') + suffixStr;
        dstName = base + suffixStr;
    }
    return dstName;
}

PXR_NS::TfToken uniqueChildName(
    const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex,
    const PXR_NS::SdfPath&                parent,
    const std::string&                    childName
)
{
    auto childPrims = sceneIndex->GetChildPrimPaths(parent);
    TfToken::HashSet existingNames;
    for (const auto& child : childPrims) {
        existingNames.insert(child.GetNameToken());
    }

    return TfToken(uniqueName(existingNames, childName));
}

} // namespace MAYAHYDRA_NS_DEF
