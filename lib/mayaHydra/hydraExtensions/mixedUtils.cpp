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

#include "mixedUtils.h"

#include <flowViewport/colorPreferences/fvpColorPreferences.h>

#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/tokens.h>
#include <mayaHydraLib/debugCodes.h>

#include <array>
#include <cmath>
#include <string>
#include <unordered_set>

#include <pxr/base/gf/vec2d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/base/gf/vec3d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/vec3i.h>
#include <pxr/base/gf/vec4d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>

#include <cstdint>

#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MHWGeometry.h>
#include <maya/MAngle.h>
#include <maya/MDataHandle.h>
#include <maya/MDistance.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>
#include <maya/MTime.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MFnDoubleArrayData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnFloatArrayData.h>
#include <maya/MFnIntArrayData.h>
#include <maya/MFnMatrixArrayData.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MFnEnumAttribute.h>
#include <maya/MFnPointArrayData.h>
#include <maya/MFnStringData.h>
#include <maya/MFnStringArrayData.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnVectorArrayData.h>
#include <maya/MDoubleArray.h>
#include <maya/MFloatArray.h>
#include <maya/MIntArray.h>
#include <maya/MMatrixArray.h>
#include <maya/MPointArray.h>
#include <maya/MStringArray.h>
#include <maya/MVectorArray.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

// OptionVar to emit enum primvars as label tokens instead of integer values.
constexpr const char* kExtEnumUseLabelsOptionVar = "mayaHydraExtensionEnumUseLabels";

// Extension attributes are defined on node types (often by plugins). Dynamic attributes
// are user-authored per-node (e.g., via addAttr) and are not part of the type definition.
bool UseEnumLabelsForExtensionAndDynamicAttrs()
{
    if (MGlobal::optionVarExists(kExtEnumUseLabelsOptionVar)) {
        return MGlobal::optionVarIntValue(kExtEnumUseLabelsOptionVar) != 0;
    }
    return false;
}

// Store value in attrs. Default check is done via MPlug::isDefaultValue() before calling.
template<typename T>
void UpdateAttrs(const char* attrName, const T& val, VtDictionary& attrs)
{
    attrs[attrName] = VtValue(val);
}

// Store value in attrs. Default check is done via MPlug::isDefaultValue() before calling.
void UpdateAttrsValue(
    const char* attrName,
    const VtValue& val,
    const VtValue& /* defaultVal */,
    const bool /* hasDefault */,
    VtDictionary& attrs)
{
    attrs[attrName] = val;
}

// Try non-networked first, then networked plug lookup.
MPlug FindPlugWithFallback(
    const MFnDependencyNode& nodeFn,
    const MString& name,
    MStatus& status)
{
    MPlug plug = nodeFn.findPlug(name, false, &status);
    if (!status) {
        plug = nodeFn.findPlug(name, true, &status);
    }
    return plug;
}

// Convert typed array data from an MObject to Vt array.
template <typename ArrayType, typename FnDataType, typename VtArrayType>
VtArrayType GetVtArrayFromObject(const MObject& obj, VtArrayType (*convert)(const ArrayType&))
{
    if (obj.isNull()) {
        return VtArrayType();
    }

    MStatus status;
    FnDataType data(obj, &status);
    if (!status) {
        return VtArrayType();
    }

    return convert(data.array());
}

// Convert typed array data from a plug to Vt array, keeping the handle alive.
template <typename ArrayType, typename FnDataType, typename VtArrayType>
VtArrayType GetVtArrayFromPlug(const MPlug& plug, VtArrayType (*convert)(const ArrayType&))
{
    MObject dataObj = plug.asMObject();
    if (dataObj.isNull()) {
        MStatus getStatus;
        plug.getValue(dataObj);
        if (dataObj.isNull()) {
            MStatus     handleStatus;
            MDataHandle handle = plug.asMDataHandle(&handleStatus);
            if (handleStatus) {
                dataObj = handle.data();
            }
        }
    }

    return GetVtArrayFromObject<ArrayType, FnDataType, VtArrayType>(dataObj, convert);
}

// Read scalar value from a plug.
template <typename Scalar>
Scalar ReadPlugValue(const MPlug& plug);

template <>
short ReadPlugValue<short>(const MPlug& plug)
{
    return plug.asShort();
}

template <>
int ReadPlugValue<int>(const MPlug& plug)
{
    return plug.asInt();
}

template <>
float ReadPlugValue<float>(const MPlug& plug)
{
    return plug.asFloat();
}

template <>
double ReadPlugValue<double>(const MPlug& plug)
{
    return plug.asDouble();
}

// Extract numeric tuple data from an MFnNumericData.
template <typename Scalar, size_t N>
struct NumericDataExtractor;

template <>
struct NumericDataExtractor<short, 2> {
    static bool Get(MFnNumericData& data, std::array<short, 2>& out)
    {
        return data.getData2Short(out[0], out[1]);
    }
};

template <>
struct NumericDataExtractor<short, 3> {
    static bool Get(MFnNumericData& data, std::array<short, 3>& out)
    {
        return data.getData3Short(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDataExtractor<int, 2> {
    static bool Get(MFnNumericData& data, std::array<int, 2>& out)
    {
        return data.getData2Int(out[0], out[1]);
    }
};

template <>
struct NumericDataExtractor<int, 3> {
    static bool Get(MFnNumericData& data, std::array<int, 3>& out)
    {
        return data.getData3Int(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDataExtractor<float, 2> {
    static bool Get(MFnNumericData& data, std::array<float, 2>& out)
    {
        return data.getData2Float(out[0], out[1]);
    }
};

template <>
struct NumericDataExtractor<float, 3> {
    static bool Get(MFnNumericData& data, std::array<float, 3>& out)
    {
        return data.getData3Float(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDataExtractor<double, 2> {
    static bool Get(MFnNumericData& data, std::array<double, 2>& out)
    {
        return data.getData2Double(out[0], out[1]);
    }
};

template <>
struct NumericDataExtractor<double, 3> {
    static bool Get(MFnNumericData& data, std::array<double, 3>& out)
    {
        return data.getData3Double(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDataExtractor<double, 4> {
    static bool Get(MFnNumericData& data, std::array<double, 4>& out)
    {
        return data.getData4Double(out[0], out[1], out[2], out[3]);
    }
};

// Extract default numeric tuple values from an MFnNumericAttribute.
template <typename Scalar, size_t N>
struct NumericDefaultExtractor;

template <>
struct NumericDefaultExtractor<short, 2> {
    static void Get(MFnNumericAttribute& attr, std::array<short, 2>& out)
    {
        int x = 0;
        int y = 0;
        attr.getDefault(x, y);
        out[0] = static_cast<short>(x);
        out[1] = static_cast<short>(y);
    }
};

template <>
struct NumericDefaultExtractor<short, 3> {
    static void Get(MFnNumericAttribute& attr, std::array<short, 3>& out)
    {
        int x = 0;
        int y = 0;
        int z = 0;
        attr.getDefault(x, y, z);
        out[0] = static_cast<short>(x);
        out[1] = static_cast<short>(y);
        out[2] = static_cast<short>(z);
    }
};

template <>
struct NumericDefaultExtractor<int, 2> {
    static void Get(MFnNumericAttribute& attr, std::array<int, 2>& out)
    {
        attr.getDefault(out[0], out[1]);
    }
};

template <>
struct NumericDefaultExtractor<int, 3> {
    static void Get(MFnNumericAttribute& attr, std::array<int, 3>& out)
    {
        attr.getDefault(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDefaultExtractor<float, 2> {
    static void Get(MFnNumericAttribute& attr, std::array<float, 2>& out)
    {
        attr.getDefault(out[0], out[1]);
    }
};

template <>
struct NumericDefaultExtractor<float, 3> {
    static void Get(MFnNumericAttribute& attr, std::array<float, 3>& out)
    {
        attr.getDefault(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDefaultExtractor<double, 2> {
    static void Get(MFnNumericAttribute& attr, std::array<double, 2>& out)
    {
        attr.getDefault(out[0], out[1]);
    }
};

template <>
struct NumericDefaultExtractor<double, 3> {
    static void Get(MFnNumericAttribute& attr, std::array<double, 3>& out)
    {
        attr.getDefault(out[0], out[1], out[2]);
    }
};

template <>
struct NumericDefaultExtractor<double, 4> {
    static void Get(MFnNumericAttribute& attr, std::array<double, 4>& out)
    {
        attr.getDefault(out[0], out[1], out[2], out[3]);
    }
};

// Build the matching GfVec type from tuple values.
template <typename Scalar, size_t N>
struct VecBuilder;

template <>
struct VecBuilder<short, 2> {
    using VecType = GfVec2i;
    static VecType Make(const std::array<short, 2>& v)
    {
        return GfVec2i(static_cast<int>(v[0]), static_cast<int>(v[1]));
    }
};

template <>
struct VecBuilder<short, 3> {
    using VecType = GfVec3i;
    static VecType Make(const std::array<short, 3>& v)
    {
        return GfVec3i(static_cast<int>(v[0]), static_cast<int>(v[1]), static_cast<int>(v[2]));
    }
};

template <>
struct VecBuilder<int, 2> {
    using VecType = GfVec2i;
    static VecType Make(const std::array<int, 2>& v)
    {
        return GfVec2i(v[0], v[1]);
    }
};

template <>
struct VecBuilder<int, 3> {
    using VecType = GfVec3i;
    static VecType Make(const std::array<int, 3>& v)
    {
        return GfVec3i(v[0], v[1], v[2]);
    }
};

template <>
struct VecBuilder<float, 2> {
    using VecType = GfVec2f;
    static VecType Make(const std::array<float, 2>& v)
    {
        return GfVec2f(v[0], v[1]);
    }
};

template <>
struct VecBuilder<float, 3> {
    using VecType = GfVec3f;
    static VecType Make(const std::array<float, 3>& v)
    {
        return GfVec3f(v[0], v[1], v[2]);
    }
};

template <>
struct VecBuilder<double, 2> {
    using VecType = GfVec2d;
    static VecType Make(const std::array<double, 2>& v)
    {
        return GfVec2d(v[0], v[1]);
    }
};

template <>
struct VecBuilder<double, 3> {
    using VecType = GfVec3d;
    static VecType Make(const std::array<double, 3>& v)
    {
        return GfVec3d(v[0], v[1], v[2]);
    }
};

template <>
struct VecBuilder<double, 4> {
    using VecType = GfVec4d;
    static VecType Make(const std::array<double, 4>& v)
    {
        return GfVec4d(v[0], v[1], v[2], v[3]);
    }
};

static const std::array<const char*, 2> kNumericSuffixes01 = {{ "0", "1" }};
static const std::array<const char*, 3> kNumericSuffixes012 = {{ "0", "1", "2" }};
static const std::array<const char*, 4> kNumericSuffixesXYZW = {{ "X", "Y", "Z", "W" }};

// Extract a single default value from a numeric attribute.
template <typename Scalar>
struct ChildDefaultExtractor;

template <>
struct ChildDefaultExtractor<short> {
    static bool Get(MFnNumericAttribute& attr, short& out)
    {
        int temp = 0;
        if (!attr.getDefault(temp)) {
            return false;
        }
        out = static_cast<short>(temp);
        return true;
    }
};

template <>
struct ChildDefaultExtractor<int> {
    static bool Get(MFnNumericAttribute& attr, int& out)
    {
        return attr.getDefault(out);
    }
};

template <>
struct ChildDefaultExtractor<float> {
    static bool Get(MFnNumericAttribute& attr, float& out)
    {
        return attr.getDefault(out);
    }
};

template <>
struct ChildDefaultExtractor<double> {
    static bool Get(MFnNumericAttribute& attr, double& out)
    {
        return attr.getDefault(out);
    }
};

// Extract compound child values and defaults from child plugs.
template <typename Scalar, size_t N>
bool GetCompoundChildValues(
    const MPlug& attrPlug,
    std::array<Scalar, N>& values,
    std::array<Scalar, N>& defaults)
{
    if (attrPlug.numChildren() < N) {
        return false;
    }

    for (size_t i = 0; i < N; ++i) {
        const MPlug child = attrPlug.child(static_cast<unsigned int>(i));
        values[i] = ReadPlugValue<Scalar>(child);
        MStatus numericStatus;
        MFnNumericAttribute childAttr(child.attribute(), &numericStatus);
        if (!numericStatus || !ChildDefaultExtractor<Scalar>::Get(childAttr, defaults[i])) {
            return false;
        }
    }

    return true;
}

// Extract numeric tuple values using child plugs or numeric data fallbacks.
template <typename Scalar, size_t N>
bool GetNumericTupleValue(
    const MPlug& attrPlug,
    const MFnDependencyNode& nodeFn,
    const MFnAttribute& attrFn,
    const std::array<const char*, N>& suffixes,
    std::array<Scalar, N>& outValues)
{
    if (attrPlug.numChildren() >= N) {
        for (size_t i = 0; i < N; ++i) {
            outValues[i] = ReadPlugValue<Scalar>(attrPlug.child(static_cast<unsigned int>(i)));
        }
        return true;
    }

    const MString baseName = attrFn.name();
    MStatus       childStatus;
    bool          foundAll = true;
    for (size_t i = 0; i < N; ++i) {
        MPlug child = FindPlugWithFallback(nodeFn, baseName + suffixes[i], childStatus);
        if (!childStatus) {
            foundAll = false;
            break;
        }
        outValues[i] = ReadPlugValue<Scalar>(child);
    }
    if (foundAll) {
        return true;
    }

    MStatus     handleStatus;
    MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
    if (handleStatus) {
        MObject       dataObj = handle.data();
        MFnNumericData numericData(dataObj, &handleStatus);
        if (handleStatus && NumericDataExtractor<Scalar, N>::Get(numericData, outValues)) {
            return true;
        }
    }

    MStatus       dataStatus;
    MObject       dataObj = attrPlug.asMObject();
    MFnNumericData numericData(dataObj, &dataStatus);
    if (dataStatus && NumericDataExtractor<Scalar, N>::Get(numericData, outValues)) {
        return true;
    }

    return false;
}

// Update attribute dictionary for numeric tuple types. Default check done via MPlug::isDefaultValue().
template <typename Scalar, size_t N>
void UpdateNumericTupleAttr(
    const char* attrName,
    const std::array<Scalar, N>& value,
    VtDictionary& attrs)
{
    attrs[attrName] = VtValue(VecBuilder<Scalar, N>::Make(value));
}

// Convert Maya string array to VtStringArray.
VtStringArray ToVtStringArray(const MStringArray& arr)
{
    VtStringArray out;
    out.reserve(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out.push_back(arr[i].asChar());
    }
    return out;
}

// Convert Maya int array to VtIntArray.
VtIntArray ToVtIntArray(const MIntArray& arr)
{
    VtIntArray out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out[i] = arr[i];
    }
    return out;
}

// Convert Maya float array to VtFloatArray.
VtFloatArray ToVtFloatArray(const MFloatArray& arr)
{
    VtFloatArray out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out[i] = arr[i];
    }
    return out;
}

// Convert Maya double array to VtDoubleArray.
VtDoubleArray ToVtDoubleArray(const MDoubleArray& arr)
{
    VtDoubleArray out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out[i] = arr[i];
    }
    return out;
}

// Convert Maya vector array to VtArray<GfVec3d>.
VtArray<GfVec3d> ToVtVec3dArray(const MVectorArray& arr)
{
    VtArray<GfVec3d> out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        const MVector& v = arr[i];
        out[i] = GfVec3d(v.x, v.y, v.z);
    }
    return out;
}

// Convert Maya point array to VtArray<GfVec4d>.
VtArray<GfVec4d> ToVtVec4dArray(const MPointArray& arr)
{
    VtArray<GfVec4d> out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        const MPoint& p = arr[i];
        out[i] = GfVec4d(p.x, p.y, p.z, p.w);
    }
    return out;
}

// Convert Maya matrix array to VtArray<GfMatrix4d>.
VtArray<GfMatrix4d> ToVtMatrix4dArray(const MMatrixArray& arr)
{
    VtArray<GfMatrix4d> out;
    out.resize(arr.length());
    for (unsigned int i = 0; i < arr.length(); ++i) {
        out[i] = GetGfMatrixFromMaya(arr[i]);
    }
    return out;
}

// Extract numeric data from a typed data object into a VtValue.
bool ExtractNumericDataFromObject(const MObject& dataObj, VtValue& outValue)
{
    if (dataObj.isNull()) {
        return false;
    }

    MStatus status;
    MFnNumericData numericData(dataObj, &status);
    if (!status) {
        return false;
    }

    switch (numericData.numericType()) {
    case MFnNumericData::k2Short: {
        short x = 0, y = 0;
        numericData.getData2Short(x, y);
        outValue = VtValue(GfVec2i(static_cast<int>(x), static_cast<int>(y)));
        return true;
    }
    case MFnNumericData::k3Short: {
        short x = 0, y = 0, z = 0;
        numericData.getData3Short(x, y, z);
        outValue = VtValue(GfVec3i(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z)));
        return true;
    }
    case MFnNumericData::k2Int: {
        int x = 0, y = 0;
        numericData.getData2Int(x, y);
        outValue = VtValue(GfVec2i(x, y));
        return true;
    }
    case MFnNumericData::k3Int: {
        int x = 0, y = 0, z = 0;
        numericData.getData3Int(x, y, z);
        outValue = VtValue(GfVec3i(x, y, z));
        return true;
    }
    case MFnNumericData::k2Float: {
        float x = 0.0f, y = 0.0f;
        numericData.getData2Float(x, y);
        outValue = VtValue(GfVec2f(x, y));
        return true;
    }
    case MFnNumericData::k3Float: {
        float x = 0.0f, y = 0.0f, z = 0.0f;
        numericData.getData3Float(x, y, z);
        outValue = VtValue(GfVec3f(x, y, z));
        return true;
    }
    case MFnNumericData::k2Double: {
        double x = 0.0, y = 0.0;
        numericData.getData2Double(x, y);
        outValue = VtValue(GfVec2d(x, y));
        return true;
    }
    case MFnNumericData::k3Double: {
        double x = 0.0, y = 0.0, z = 0.0;
        numericData.getData3Double(x, y, z);
        outValue = VtValue(GfVec3d(x, y, z));
        return true;
    }
    case MFnNumericData::k4Double: {
        double x = 0.0, y = 0.0, z = 0.0, w = 0.0;
        numericData.getData4Double(x, y, z, w);
        outValue = VtValue(GfVec4d(x, y, z, w));
        return true;
    }
    // Scalar types (e.g. Arnold aiSubdivIterations, aiAutobumpVisibility).
    // MFnNumericData lacks single-value getters; use getData2* and use first component.
    case MFnNumericData::kBoolean: {
        int x = 0, y = 0;
        if (numericData.getData2Int(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(static_cast<bool>(x));
        return true;
    }
    case MFnNumericData::kByte:
    case MFnNumericData::kChar: {
        int x = 0, y = 0;
        if (numericData.getData2Int(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(static_cast<int>(static_cast<char>(x)));
        return true;
    }
    case MFnNumericData::kShort: {
        short x = 0, y = 0;
        if (numericData.getData2Short(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(x);
        return true;
    }
    case MFnNumericData::kInt: {
        int x = 0, y = 0;
        if (numericData.getData2Int(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(x);
        return true;
    }
    case MFnNumericData::kInt64: {
        double x = 0.0, y = 0.0;
        if (numericData.getData2Double(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(static_cast<MInt64>(x));
        return true;
    }
    case MFnNumericData::kFloat: {
        float x = 0.0f, y = 0.0f;
        if (numericData.getData2Float(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(x);
        return true;
    }
    case MFnNumericData::kDouble: {
        double x = 0.0, y = 0.0;
        if (numericData.getData2Double(x, y) != MStatus::kSuccess) {
            return false;
        }
        outValue = VtValue(x);
        return true;
    }
    default:
        return false;
    }
}

// Extract numeric data from a plug, using handles when possible.
bool ExtractNumericDataFromPlug(
    const MObject& dataObj,
    const MPlug& plug,
    VtValue& outValue,
    bool& supportsDefault)
{
    supportsDefault = false;
    if (dataObj.isNull()) {
        return false;
    }

    MStatus status;
    MFnNumericData numericData(dataObj, &status);
    if (!status) {
        return false;
    }

    switch (numericData.numericType()) {
    case MFnNumericData::kBoolean: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asBool());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kByte:
    case MFnNumericData::kChar: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(static_cast<int>(handle.asChar()));
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kShort: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asShort());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kInt: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asInt());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kInt64: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asInt64());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kFloat: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asFloat());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kDouble: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asDouble());
        supportsDefault = true;
        return true;
    }
    case MFnNumericData::kAddr: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        void*  ptr = handle.asAddr();
        MInt64 val = static_cast<MInt64>(reinterpret_cast<intptr_t>(ptr));
        outValue = VtValue(val);
        supportsDefault = true;
        return true;
    }
    default:
        supportsDefault = true;
        return ExtractNumericDataFromObject(dataObj, outValue);
    }
}

// Resolve the file texture path for a Maya file node.
TfToken GetFileTexturePath(const MFnDependencyNode& fileNode)
{
    if (fileNode.findPlug(MayaAttrs::file::uvTilingMode, true).asShort() != 0) {
        const TfToken ret {
            fileNode.findPlug(MayaAttrs::file::fileTextureNamePattern, true).asString().asChar()
        };
        return ret.IsEmpty()
            ? TfToken { fileNode.findPlug(MayaAttrs::file::computedFileTextureNamePattern, true)
                            .asString()
                            .asChar() }
            : ret;
    } else {
        const TfToken ret { MRenderUtil::exactFileTextureName(fileNode.object()).asChar() };
        return ret.IsEmpty() ? TfToken { fileNode.findPlug(MayaAttrs::file::fileTextureName, true)
                                             .asString()
                                             .asChar() }
                             : ret;
    }
}

// Check whether the DAG path refers to a shape with transform parent.
bool IsShape(const MDagPath& dagPath)
{
    if (dagPath.hasFn(MFn::kTransform)) {
        return false;
    }

    // go to the parent
    MDagPath parentDagPath = dagPath;
    parentDagPath.pop();
    if (!parentDagPath.hasFn(MFn::kTransform)) {
        return false;
    }

    unsigned int numberOfShapesDirectlyBelow = 0;
    parentDagPath.numberOfShapesDirectlyBelow(numberOfShapesDirectlyBelow);
    return (numberOfShapesDirectlyBelow == 1);
}

// Convert a DAG path to a sanitized SdfPath.
SdfPath DagPathToSdfPath(
    const MDagPath& dagPath,
    const bool      mergeTransformAndShape,
    const bool      stripNamespaces)
{
    std::string name = dagPath.fullPathName().asChar();
    if ( name.empty() ) {
        MFnDependencyNode dep(dagPath.node());
        if (dep.name().length() || dep.typeName().length()){
            TF_WARN("Empty fullpath name for DAG object : %s of type : %s", dep.name().asChar(), dep.typeName().asChar());
        }
        return SdfPath();
    }
    SanitizeNameForSdfPath(name, stripNamespaces);
    SdfPath usdPath(name);

    if (mergeTransformAndShape && IsShape(dagPath)) {
        usdPath = usdPath.GetParentPath();
    }

    return usdPath;
}

// Convert a render item name/id into a valid SdfPath.
SdfPath RenderItemToSdfPath(const MRenderItem& ri, const bool stripNamespaces)
{
    std::string internalObjectId(
        "_" + std::to_string(ri.InternalObjectId())); // preventively prepend item id by underscore
    std::string name(ri.name().asChar() + internalObjectId);
    // Try to sanitize maya path to be used as an sdf path.
    SanitizeNameForSdfPath(name, stripNamespaces);
    // Path names must start with a letter, not a number
    // If a number is found, prepend the path with an underscore
    char digit = name[0];
    if (std::isdigit(digit)) {
        name.insert(0, "_");
    }

    SdfPath sdfPath(name);
    if (!TF_VERIFY(
            !sdfPath.IsEmpty(),
            "Render item using invalid SdfPath '%s'. Using item's id instead.",
            name.c_str())) {
        // If failed to include render item's name as an SdfPath simply use the item id.
        return SdfPath(internalObjectId);
    }
    return sdfPath;
}

// Query a display RGB color preference into RGBA.
bool getRGBAColorPreferenceValue(const std::string& colorName, GfVec4f& outColor)
{
    MDoubleArray rgbaColorValues;
    bool         wasCommandSuccessful = MGlobal::executeCommand(
        MString("displayRGBColor -q -a ") + MString(colorName.c_str()), rgbaColorValues);
    if (!wasCommandSuccessful || rgbaColorValues.length() != 4) {
        return false;
    }
    outColor[0] = static_cast<float>(rgbaColorValues[0]);
    outColor[1] = static_cast<float>(rgbaColorValues[1]);
    outColor[2] = static_cast<float>(rgbaColorValues[2]);
    outColor[3] = static_cast<float>(rgbaColorValues[3]);
    return true;
}

// Resolve a color name to its palette index.
bool getIndexedColorPreferenceIndex(
    const std::string& colorName,
    const std::string& tableName,
    size_t&            outIndex)
{
    MIntArray   indexInPalette;
    std::string getIndexCommand = "displayColor -q -" + tableName + " " + colorName;
    bool        wasCommandSuccessful
        = MGlobal::executeCommand(MString(getIndexCommand.c_str()), indexInPalette);
    if (!wasCommandSuccessful || indexInPalette.length() != 1) {
        return false;
    }
    outIndex = indexInPalette[0];
    return true;
}

// Fetch palette color by index as RGBA.
bool getColorPreferencesPaletteColor(
    const std::string& tableName,
    size_t             index,
    GfVec4f&   outColor)
{
    MDoubleArray rgbColorValues;
    std::string  getColorCommand = "colorIndex -q -" + tableName + " " + std::to_string(index);
    bool         wasCommandSuccessful
        = MGlobal::executeCommand(MString(getColorCommand.c_str()), rgbColorValues);
    if (!wasCommandSuccessful || rgbColorValues.length() != 3) {
        return false;
    }
    outColor[0] = static_cast<float>(rgbColorValues[0]);
    outColor[1] = static_cast<float>(rgbColorValues[1]);
    outColor[2] = static_cast<float>(rgbColorValues[2]);
    outColor[3] = 1.0f;
    return true;
}

// Fetch palette color by name via index lookup.
bool getIndexedColorPreferenceValue(
    const std::string& colorName,
    const std::string& tableName,
    GfVec4f&   outColor)
{
    size_t colorIndex = 0;
    if (getIndexedColorPreferenceIndex(colorName, tableName, colorIndex)) {
        return getColorPreferencesPaletteColor(tableName, colorIndex, outColor);
    }
    return false;
}

// Build a unique scene index path prefix for a Maya node.
SdfPath sceneIndexPathPrefix(
    const HdSceneIndexBaseRefPtr& sceneIndex,
    MObject&                      mayaNode
)
{
    constexpr char kSceneIndexPluginSuffix[] = {"_PluginNode"};
    MFnDependencyNode dependNodeFn(mayaNode);
    // To match plugin TfType registration, name must begin with upper case.
    const auto sceneIndexPluginName = [&](){
        std::string name = dependNodeFn.typeName().asChar();
        name[0] = toupper(name[0]);
        name += kSceneIndexPluginSuffix;
        return TfToken(name);}();

    // Create a unique scene index path prefix by starting with the
    // Dag node name, and checking for uniqueness under the scene
    // index plugin parent rprim.  If not unique, add an
    // incrementing numerical suffix until it is.
    const auto sceneIndexPluginPath = SdfPath::AbsoluteRootPath().AppendChild(sceneIndexPluginName);
    const auto newName = uniqueChildName(
        sceneIndex,
        sceneIndexPluginPath,
        SanitizeNameForSdfPath(dependNodeFn.name().asChar())
    );

    return sceneIndexPluginPath.AppendChild(newName);
}

// Read a color preference token into a GfVec4f.
PXR_NS::GfVec4f getPreferencesColor(const PXR_NS::TfToken& token)
{
    PXR_NS::GfVec4f color;
    Fvp::ColorPreferences::getInstance().getColor(token, color);
    return color;
}

// Return the active geometry subsets pick mode option.
PXR_NS::TfToken GetGeomSubsetsPickMode()
{
    static const MString kOptionVarName(MayaHydraPickOptionVars->GeomSubsetsPickMode.GetText());

    if (MGlobal::optionVarExists(kOptionVarName)) {
        return TfToken(MGlobal::optionVarStringValue(kOptionVarName).asChar());
    }

    return GeomSubsetsPickModeTokens->None;
}

// Built-in Maya attributes (and light-specific non-meaningful attrs) to skip when
// includeAllAttributes is true (e.g. lights). worldPosition and isHierarchicalConnection
// are derived/internal and not meaningful to translate to Hydra primvars.
static const std::unordered_set<std::string>& _GetBuiltInAttributeSkipSet()
{
    static const std::unordered_set<std::string> skipSet = {
        "caching", "nodeState", "message", "visibility", "worldMatrix",
        "parentMatrix", "xformMatrix", "matrix", "instObjGroups",
        "intermediateObject", "overrideEnabled", "overrideVisibility",
        "inverseMatrix", "isHistoricallyInteresting",
        "worldPosition", "isHierarchicalConnection"};
    return skipSet;
}

// Manual default-value check for extension attributes. MPlug::isDefaultValue() can be
// unreliable for plugin-defined attributes, so we compare against the
// attribute's registered default. Returns true if value equals default; false if not
// or if we cannot determine.
static bool ExtensionAttrValueEqualsDefault(const MPlug& attrPlug, const MObject& attrObj)
{
    MStatus status;
    // Multi/array attributes: empty array is typically default.
    if (attrPlug.isArray()) {
        if (attrPlug.numElements() == 0) {
            return true;
        }
        // Single element: compare against the attribute's registered default.
        if (attrPlug.numElements() == 1) {
            MPlug elementPlug = attrPlug.elementByLogicalIndex(0);
            const unsigned int n = elementPlug.numChildren();
            if (n >= 2 && n <= 4) {
                MStatus numericStatus;
                MFnNumericAttribute firstChild(elementPlug.child(0).attribute(), &numericStatus);
                if (numericStatus) {
                    const auto unitType = firstChild.unitType();
                    if (n == 3 && unitType == MFnNumericData::kFloat) {
                        std::array<float, 3> value = {{ 0.0f, 0.0f, 0.0f }};
                        std::array<float, 3> defaultValue = {{ 0.0f, 0.0f, 0.0f }};
                        if (GetCompoundChildValues<float, 3>(elementPlug, value, defaultValue)) {
                            for (size_t i = 0; i < 3; ++i) {
                                if (std::abs(value[i] - defaultValue[i]) > 1e-6f) {
                                    return false;
                                }
                            }
                            return true;
                        }
                    } else if (n == 3 && unitType == MFnNumericData::kDouble) {
                        std::array<double, 3> value = {{ 0.0, 0.0, 0.0 }};
                        std::array<double, 3> defaultValue = {{ 0.0, 0.0, 0.0 }};
                        if (GetCompoundChildValues<double, 3>(elementPlug, value, defaultValue)) {
                            for (size_t i = 0; i < 3; ++i) {
                                if (std::abs(value[i] - defaultValue[i]) > 1e-9) {
                                    return false;
                                }
                            }
                            return true;
                        }
                    } else if (n == 3 && (unitType == MFnNumericData::kShort || unitType == MFnNumericData::kInt)) {
                        std::array<int, 3> value = {{ 0, 0, 0 }};
                        std::array<int, 3> defaultValue = {{ 0, 0, 0 }};
                        if (GetCompoundChildValues<int, 3>(elementPlug, value, defaultValue)) {
                            return value == defaultValue;
                        }
                    }
                }
            }
        }
    }
    switch (attrObj.apiType()) {
        case MFn::kEnumAttribute: {
            MFnEnumAttribute enumAttr(attrObj);
            short value = attrPlug.asShort();
            short defaultVal = 0;
            return enumAttr.getDefault(defaultVal) && value == defaultVal;
        }
        case MFn::kAttribute2Short:
        case MFn::kAttribute2Int:
        case MFn::kAttribute3Short:
        case MFn::kAttribute3Int:
        case MFn::kNumericAttribute: {
            MFnNumericAttribute numericAttr(attrObj);
            switch (numericAttr.unitType()) {
                case MFnNumericData::kBoolean: {
                    bool def = false;
                    return numericAttr.getDefault(def) && attrPlug.asBool() == def;
                }
                case MFnNumericData::kByte:
                case MFnNumericData::kChar: {
                    char def = 0;
                    return numericAttr.getDefault(def) && attrPlug.asChar() == def;
                }
                case MFnNumericData::kShort: {
                    int def = 0;
                    return numericAttr.getDefault(def) && attrPlug.asShort() == static_cast<short>(def);
                }
                case MFnNumericData::kInt: {
                    int def = 0;
                    return numericAttr.getDefault(def) && attrPlug.asInt() == def;
                }
                case MFnNumericData::kFloat: {
                    float def = 0.0f;
                    if (numericAttr.getDefault(def)
                        && std::abs(attrPlug.asFloat() - def) < 1e-6f) {
                        return true;
                    }
                    return false;
                }
                case MFnNumericData::kDouble: {
                    double def = 0.0;
                    if (numericAttr.getDefault(def)
                        && std::abs(attrPlug.asDouble() - def) < 1e-9) {
                        return true;
                    }
                    return false;
                }
                default:
                    return false;
            }
        }
        case MFn::kTypedAttribute: {
            MFnTypedAttribute typeAttr(attrObj);
            if (typeAttr.attrType() != MFnData::kNumeric) {
                return false;
            }
            MObject valueObj = attrPlug.asMObject();
            if (valueObj.isNull()) {
                MStatus getStatus;
                attrPlug.getValue(valueObj);
            }
            if (valueObj.isNull()) {
                return false;
            }
            MObject defaultObj;
            if (!typeAttr.getDefault(defaultObj) || defaultObj.isNull()) {
                return false;
            }
            VtValue value;
            VtValue defaultVal;
            if (!ExtractNumericDataFromObject(valueObj, value)
                || !ExtractNumericDataFromObject(defaultObj, defaultVal)) {
                return false;
            }
            return value == defaultVal;
        }
        case MFn::kCompoundAttribute: {
            const unsigned int childCount = attrPlug.numChildren();
            if (childCount < 2 || childCount > 4) {
                return false;
            }
            MFnNumericAttribute firstChild(attrPlug.child(0).attribute(), &status);
            if (!status) {
                return false;
            }
            const auto unitType = firstChild.unitType();
            if (unitType == MFnNumericData::kShort || unitType == MFnNumericData::kInt) {
                if (childCount == 3) {
                    std::array<int, 3> value = {{ 0, 0, 0 }};
                    std::array<int, 3> defaultValue = {{ 0, 0, 0 }};
                    if (!GetCompoundChildValues<int, 3>(attrPlug, value, defaultValue)) {
                        return false;
                    }
                    return value == defaultValue;
                }
            } else if (unitType == MFnNumericData::kFloat || unitType == MFnNumericData::kDouble) {
                if (childCount == 3) {
                    std::array<double, 3> value = {{ 0.0, 0.0, 0.0 }};
                    std::array<double, 3> defaultValue = {{ 0.0, 0.0, 0.0 }};
                    if (!GetCompoundChildValues<double, 3>(attrPlug, value, defaultValue)) {
                        return false;
                    }
                    for (size_t i = 0; i < 3; ++i) {
                        if (std::abs(value[i] - defaultValue[i]) > 1e-9) {
                            return false;
                        }
                    }
                    return true;
                }
            }
            return false;
        }
        default:
            return false;
    }
}

// Populate a dictionary with attribute values for translation to Hydra primvars.
// When includeAllAttributes is false: only extension and dynamic attributes.
// When true: all non-builtin attributes (skip default values in both cases).
void GetAttributesFromNode(
    const MObject& node, PXR_NS::VtDictionary& attrs, bool includeAllAttributes)
{
    attrs.clear();
    MStatus           status;
    MFnDependencyNode nodeFn(node, &status);
    if (ARCH_UNLIKELY(!status)) {
        return;
    }

    const auto& builtInSkipSet = _GetBuiltInAttributeSkipSet();

    for (size_t i = 0; i < nodeFn.attributeCount(); i++) {
        MObject      attrObj = nodeFn.attribute(i);
        MFnAttribute attrFn(attrObj);
        auto         attrName = attrFn.name().asChar();
        MStatus      plugStatus;
        MPlug        attrPlug = FindPlugWithFallback(nodeFn, attrFn.name(), plugStatus);
        if (!plugStatus || attrPlug.isNull())
            continue;
        if (attrPlug.isChild())
            continue;

        const bool isExtOrDynamic = attrFn.isExtension() || attrFn.isDynamic();
        if (!includeAllAttributes && !isExtOrDynamic)
            continue;
        if (includeAllAttributes && builtInSkipSet.count(attrName) > 0)
            continue;

        // When includeAllAttributes: always check default (skip if at default).
        // When ext/dynamic only: dynamic attrs don't check default.
        // For extension attributes, MPlug::isDefaultValue() can be unreliable
        // (plugins may not implement it), so we also use manual comparison.
        const bool ignoreDefault = includeAllAttributes ? false : attrFn.isDynamic();
        if (!ignoreDefault) {
            if (attrPlug.isDefaultValue()) {
                continue;
            }
            if (attrFn.isExtension() && ExtensionAttrValueEqualsDefault(attrPlug, attrObj)) {
                continue;
            }
        }

        switch (attrObj.apiType()) {
            case MFn::kEnumAttribute: {
                MFnEnumAttribute enumAttr(attrObj);
                short            value = attrPlug.asShort();
                short            defaultVal = 0;
                enumAttr.getDefault(defaultVal);
                if (UseEnumLabelsForExtensionAndDynamicAttrs()) {
                    const MString label = enumAttr.fieldName(value);
                    const MString defaultLabel = enumAttr.fieldName(defaultVal);
                    if (!label.length() || !defaultLabel.length()) {
                        UpdateAttrsValue(
                            attrName,
                            VtValue(value),
                            VtValue(defaultVal),
                            !ignoreDefault,
                            attrs);
                    } else {
                        UpdateAttrsValue(
                            attrName,
                            VtValue(TfToken(label.asChar())),
                            VtValue(TfToken(defaultLabel.asChar())),
                            !ignoreDefault,
                            attrs);
                    }
                } else {
                    UpdateAttrsValue(
                        attrName,
                        VtValue(value),
                        VtValue(defaultVal),
                        !ignoreDefault,
                        attrs);
                }
            } break;
            case MFn::kTypedAttribute: {
                MFnTypedAttribute typeAttr(attrObj);
                switch (typeAttr.attrType()) {
                case MFnData::kString: {
                    std::string value = attrPlug.asString().asChar();
                    MObject defaultValObj;
                    typeAttr.getDefault(defaultValObj);
                    MFnStringData defaultStringData(defaultValObj);
                    std::string   defaultVal = defaultStringData.string().asChar();
                    UpdateAttrsValue(
                        attrName,
                        VtValue(value),
                        VtValue(defaultVal),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kStringArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MStringArray,
                        MFnStringArrayData,
                        VtStringArray>(attrPlug, ToVtStringArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MStringArray,
                        MFnStringArrayData,
                        VtStringArray>(defaultObj, ToVtStringArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kIntArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MIntArray,
                        MFnIntArrayData,
                        VtIntArray>(attrPlug, ToVtIntArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MIntArray,
                        MFnIntArrayData,
                        VtIntArray>(defaultObj, ToVtIntArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kFloatArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MFloatArray,
                        MFnFloatArrayData,
                        VtFloatArray>(attrPlug, ToVtFloatArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MFloatArray,
                        MFnFloatArrayData,
                        VtFloatArray>(defaultObj, ToVtFloatArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kDoubleArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MDoubleArray,
                        MFnDoubleArrayData,
                        VtDoubleArray>(attrPlug, ToVtDoubleArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MDoubleArray,
                        MFnDoubleArrayData,
                        VtDoubleArray>(defaultObj, ToVtDoubleArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kVectorArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MVectorArray,
                        MFnVectorArrayData,
                        VtArray<GfVec3d>>(attrPlug, ToVtVec3dArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MVectorArray,
                        MFnVectorArrayData,
                        VtArray<GfVec3d>>(defaultObj, ToVtVec3dArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kPointArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MPointArray,
                        MFnPointArrayData,
                        VtArray<GfVec4d>>(attrPlug, ToVtVec4dArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MPointArray,
                        MFnPointArrayData,
                        VtArray<GfVec4d>>(defaultObj, ToVtVec4dArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kMatrix: {
                    MStatus matrixStatus;
                    MObject dataObj = attrPlug.asMObject();
                    if (dataObj.isNull()) {
                        attrPlug.getValue(dataObj);
                    }
                    if (dataObj.isNull()) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            dataObj = handle.data();
                        }
                    }
                    if (dataObj.isNull()) {
                        break;
                    }

                    MFnMatrixData matrixData(dataObj, &matrixStatus);
                    if (!matrixStatus) {
                        break;
                    }

                    GfMatrix4d value = GetGfMatrixFromMaya(matrixData.matrix(&matrixStatus));

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    if (defaultObj.isNull()) {
                        UpdateAttrsValue(attrName, VtValue(value), VtValue(), false, attrs);
                        break;
                    }

                    MFnMatrixData defaultData(defaultObj, &matrixStatus);
                    if (!matrixStatus) {
                        UpdateAttrsValue(attrName, VtValue(value), VtValue(), false, attrs);
                        break;
                    }

                    GfMatrix4d defaultVal = GetGfMatrixFromMaya(defaultData.matrix(&matrixStatus));
                    UpdateAttrsValue(
                        attrName,
                        VtValue(value),
                        VtValue(defaultVal),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kMatrixArray: {
                    const auto valueArray = GetVtArrayFromPlug<
                        MMatrixArray,
                        MFnMatrixArrayData,
                        VtArray<GfMatrix4d>>(attrPlug, ToVtMatrix4dArray);

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray = GetVtArrayFromObject<
                        MMatrixArray,
                        MFnMatrixArrayData,
                        VtArray<GfMatrix4d>>(defaultObj, ToVtMatrix4dArray);

                    UpdateAttrsValue(
                        attrName,
                        VtValue(valueArray),
                        VtValue(defaultArray),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnData::kNumeric: {
                    VtValue value;
                    bool supportsDefault = false;
                    if (!ExtractNumericDataFromPlug(attrPlug.asMObject(), attrPlug, value, supportsDefault)) {
                        TF_DEBUG(MAYAHYDRALIB_GET_EXTENSION_ATTRS)
                            .Msg(
                                "Not handled custom attribute: name=%s, type=kNumeric\n",
                                attrFn.name().asChar());
                        break;
                    }
                    attrs[attrName] = value;
                } break;
                default:
                    // TODO: Add more types if necessary
                    TF_DEBUG(MAYAHYDRALIB_GET_EXTENSION_ATTRS)
                        .Msg(
                            "Not handled custom attribute: name=%s, type=kTypedAttribute\n",
                            attrFn.name().asChar());
                    break;
                }
                break;
            } break;
            case MFn::kCompoundAttribute: {
                // For multi attributes (e.g. aiLookAt), use first element if array has one.
                MPlug compoundPlug = attrPlug;
                if (attrPlug.isArray() && attrPlug.numElements() >= 1) {
                    compoundPlug = attrPlug.elementByLogicalIndex(0);
                }
                const unsigned int childCount = compoundPlug.numChildren();
                if (childCount < 2) {
                    break;
                }

                MStatus numericStatus;
                MFnNumericAttribute firstChild(compoundPlug.child(0).attribute(), &numericStatus);
                if (!numericStatus) {
                    break;
                }
                const auto unitType = firstChild.unitType();
                bool       allSame = true;
                for (unsigned int i = 1; i < childCount; ++i) {
                    MFnNumericAttribute childAttr(compoundPlug.child(i).attribute(), &numericStatus);
                    if (!numericStatus || childAttr.unitType() != unitType) {
                        allSame = false;
                        break;
                    }
                }
                if (!numericStatus || !allSame) {
                    break;
                }

                switch (unitType) {
                case MFnNumericData::kShort:
                case MFnNumericData::kInt: {
                    const bool isShort = (unitType == MFnNumericData::kShort);
                    if (childCount == 2) {
                        if (isShort) {
                            std::array<short, 2> value = {{ 0, 0 }};
                            std::array<short, 2> defaultValue = {{ 0, 0 }};
                            if (!GetCompoundChildValues<short, 2>(compoundPlug, value, defaultValue)) {
                                break;
                            }
                            UpdateNumericTupleAttr<short, 2>(attrName, value, attrs);
                        } else {
                            std::array<int, 2> value = {{ 0, 0 }};
                            std::array<int, 2> defaultValue = {{ 0, 0 }};
                            if (!GetCompoundChildValues<int, 2>(compoundPlug, value, defaultValue)) {
                                break;
                            }
                            UpdateNumericTupleAttr<int, 2>(attrName, value, attrs);
                        }
                    } else if (childCount == 3) {
                        if (isShort) {
                            std::array<short, 3> value = {{ 0, 0, 0 }};
                            std::array<short, 3> defaultValue = {{ 0, 0, 0 }};
                            if (!GetCompoundChildValues<short, 3>(compoundPlug, value, defaultValue)) {
                                break;
                            }
                            UpdateNumericTupleAttr<short, 3>(attrName, value, attrs);
                        } else {
                            std::array<int, 3> value = {{ 0, 0, 0 }};
                            std::array<int, 3> defaultValue = {{ 0, 0, 0 }};
                            if (!GetCompoundChildValues<int, 3>(compoundPlug, value, defaultValue)) {
                                break;
                            }
                            UpdateNumericTupleAttr<int, 3>(attrName, value, attrs);
                        }
                    }
                } break;
                case MFnNumericData::kFloat: {
                    if (childCount == 2) {
                        std::array<float, 2> value = {{ 0.0f, 0.0f }};
                        std::array<float, 2> defaultValue = {{ 0.0f, 0.0f }};
                        if (!GetCompoundChildValues<float, 2>(compoundPlug, value, defaultValue)) {
                            break;
                        }
                        UpdateNumericTupleAttr<float, 2>(attrName, value, attrs);
                    } else if (childCount == 3) {
                        std::array<float, 3> value = {{ 0.0f, 0.0f, 0.0f }};
                        std::array<float, 3> defaultValue = {{ 0.0f, 0.0f, 0.0f }};
                        if (!GetCompoundChildValues<float, 3>(compoundPlug, value, defaultValue)) {
                            break;
                        }
                        UpdateNumericTupleAttr<float, 3>(attrName, value, attrs);
                    }
                } break;
                case MFnNumericData::kDouble: {
                    if (childCount == 2) {
                        std::array<double, 2> value = {{ 0.0, 0.0 }};
                        std::array<double, 2> defaultValue = {{ 0.0, 0.0 }};
                        if (!GetCompoundChildValues<double, 2>(compoundPlug, value, defaultValue)) {
                            break;
                        }
                        UpdateNumericTupleAttr<double, 2>(attrName, value, attrs);
                    } else if (childCount == 3) {
                        std::array<double, 3> value = {{ 0.0, 0.0, 0.0 }};
                        std::array<double, 3> defaultValue = {{ 0.0, 0.0, 0.0 }};
                        if (!GetCompoundChildValues<double, 3>(compoundPlug, value, defaultValue)) {
                            break;
                        }
                        UpdateNumericTupleAttr<double, 3>(attrName, value, attrs);
                    } else if (childCount == 4) {
                        std::array<double, 4> value = {{ 0.0, 0.0, 0.0, 0.0 }};
                        std::array<double, 4> defaultValue = {{ 0.0, 0.0, 0.0, 0.0 }};
                        if (!GetCompoundChildValues<double, 4>(compoundPlug, value, defaultValue)) {
                            break;
                        }
                        UpdateNumericTupleAttr<double, 4>(attrName, value, attrs);
                    }
                } break;
                default:
                    break;
                }
            } break;
            case MFn::kAttribute2Double:
            case MFn::kAttribute2Float:
            case MFn::kAttribute2Short:
            case MFn::kAttribute2Int:
            case MFn::kAttribute3Double:
            case MFn::kAttribute3Float:
            case MFn::kAttribute3Short:
            case MFn::kAttribute3Int:
            case MFn::kAttribute4Double:
            case MFn::kNumericAttribute: {
                MFnNumericAttribute numericAttr(attrObj);
                switch (numericAttr.unitType()) {
                case MFnNumericData::kBoolean: {
                    auto value = attrPlug.asBool();
                    UpdateAttrs<bool>(attrName, value, attrs);
                } break;
                case MFnNumericData::kByte:
                case MFnNumericData::kChar: {
                    auto value = static_cast<int>(attrPlug.asChar());
                    UpdateAttrs<int>(attrName, value, attrs);
                } break;
                case MFnNumericData::kShort: {
                    auto  value = attrPlug.asShort();
                    UpdateAttrs<short>(attrName, value, attrs);
                } break;
                case MFnNumericData::kInt: {
                    auto value = attrPlug.asInt();
                    UpdateAttrs<int>(attrName, value, attrs);
                } break;
                case MFnNumericData::kFloat: {
                    auto  value = attrPlug.asFloat();
                    UpdateAttrs<float>(attrName, value, attrs);
                } break;
                case MFnNumericData::kDouble: {
                    auto   value = attrPlug.asDouble();
                    UpdateAttrs<double>(attrName, value, attrs);
                } break;
                case MFnNumericData::kInt64: {
                    MInt64 value = attrPlug.asInt64();
                    MInt64 defaultVal = 0;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(value),
                        VtValue(defaultVal),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::kAddr: {
                    void* defaultPtr = nullptr;
                    numericAttr.getDefault(defaultPtr);
                    const MInt64 defaultVal = static_cast<MInt64>(reinterpret_cast<intptr_t>(defaultPtr));

                    MStatus     handleStatus;
                    MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                    MInt64      value = defaultVal;
                    if (handleStatus) {
                        void* ptr = handle.asAddr();
                        value = static_cast<MInt64>(reinterpret_cast<intptr_t>(ptr));
                    }

                    UpdateAttrsValue(
                        attrName,
                        VtValue(value),
                        VtValue(defaultVal),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k2Short: {
                    std::array<short, 2> value = {{ 0, 0 }};
                    if (!GetNumericTupleValue<short, 2>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes01, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<short, 2>(attrName, value, attrs);
                } break;
                case MFnNumericData::k3Short: {
                    std::array<short, 3> value = {{ 0, 0, 0 }};
                    if (!GetNumericTupleValue<short, 3>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes012, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<short, 3>(attrName, value, attrs);
                } break;
                case MFnNumericData::k2Int: {
                    std::array<int, 2> value = {{ 0, 0 }};
                    if (!GetNumericTupleValue<int, 2>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes01, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<int, 2>(attrName, value, attrs);
                } break;
                case MFnNumericData::k3Int: {
                    std::array<int, 3> value = {{ 0, 0, 0 }};
                    if (!GetNumericTupleValue<int, 3>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes012, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<int, 3>(attrName, value, attrs);
                } break;
                case MFnNumericData::k2Float: {
                    std::array<float, 2> value = {{ 0.0f, 0.0f }};
                    if (!GetNumericTupleValue<float, 2>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes01, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<float, 2>(attrName, value, attrs);
                } break;
                case MFnNumericData::k3Float: {
                    std::array<float, 3> value = {{ 0.0f, 0.0f, 0.0f }};
                    if (!GetNumericTupleValue<float, 3>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes012, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<float, 3>(attrName, value, attrs);
                } break;
                case MFnNumericData::k2Double: {
                    std::array<double, 2> value = {{ 0.0, 0.0 }};
                    if (!GetNumericTupleValue<double, 2>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes01, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<double, 2>(attrName, value, attrs);
                } break;
                case MFnNumericData::k3Double: {
                    std::array<double, 3> value = {{ 0.0, 0.0, 0.0 }};
                    if (!GetNumericTupleValue<double, 3>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixes012, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<double, 3>(attrName, value, attrs);
                } break;
                case MFnNumericData::k4Double: {
                    std::array<double, 4> value = {{ 0.0, 0.0, 0.0, 0.0 }};
                    if (!GetNumericTupleValue<double, 4>(
                            attrPlug, nodeFn, attrFn, kNumericSuffixesXYZW, value)) {
                        break;
                    }
                    UpdateNumericTupleAttr<double, 4>(attrName, value, attrs);
                } break;
                default:
                    // TODO: Add more types if necessary
                    TF_DEBUG(MAYAHYDRALIB_GET_EXTENSION_ATTRS)
                        .Msg(
                            "Not handled custom attribute: name=%s, type=kNumericAttribute\n",
                            attrFn.name().asChar());
                    break;
                }
                break;
            } break;
            case MFn::kDoubleAngleAttribute:
            case MFn::kFloatAngleAttribute:
            case MFn::kDoubleLinearAttribute:
            case MFn::kFloatLinearAttribute:
            case MFn::kTimeAttribute:
            case MFn::kUnitAttribute: {
                MFnUnitAttribute unitAttr(attrObj);
                double           value = 0.0;
                double           defaultVal = 0.0;
                switch (unitAttr.unitType()) {
                case MFnUnitAttribute::kAngle: {
                    MAngle angleVal;
                    MAngle defaultAngle;
                    unitAttr.getDefault(defaultAngle);
                    defaultVal = defaultAngle.asRadians();
                    value = attrPlug.getValue(angleVal)
                        ? angleVal.asRadians()
                        : defaultVal;
                } break;
                case MFnUnitAttribute::kDistance: {
                    MDistance distanceVal;
                    MDistance defaultDistance;
                    unitAttr.getDefault(defaultDistance);
                    defaultVal = defaultDistance.asCentimeters();
                    value = attrPlug.getValue(distanceVal)
                        ? distanceVal.asCentimeters()
                        : defaultVal;
                } break;
                case MFnUnitAttribute::kTime: {
                    MTime timeVal;
                    MTime defaultTime;
                    unitAttr.getDefault(defaultTime);
                    defaultVal = defaultTime.asUnits(MTime::kSeconds);
                    value = attrPlug.getValue(timeVal)
                        ? timeVal.asUnits(MTime::kSeconds)
                        : defaultVal;
                } break;
                default:
                    break;
                }
                UpdateAttrsValue(
                    attrName,
                    VtValue(value),
                    VtValue(defaultVal),
                    !ignoreDefault,
                    attrs);
            } break;
            case MFn::kMatrixAttribute: {
                MStatus          matrixStatus;
                MObject          dataObj = attrPlug.asMObject();
                if (dataObj.isNull()) {
                    attrPlug.getValue(dataObj);
                }
                if (dataObj.isNull()) {
                    MStatus     handleStatus;
                    MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                    if (handleStatus) {
                        dataObj = handle.data();
                    }
                }
                if (dataObj.isNull()) {
                    break;
                }

                MFnMatrixData matrixData(dataObj, &matrixStatus);
                if (!matrixStatus) {
                    break;
                }

                GfMatrix4d value = GetGfMatrixFromMaya(matrixData.matrix(&matrixStatus));

                MFnMatrixAttribute matrixAttr(attrObj);
                MMatrix            defaultMatrix;
                matrixAttr.getDefault(defaultMatrix);
                GfMatrix4d defaultVal = GetGfMatrixFromMaya(defaultMatrix);

                UpdateAttrsValue(
                    attrName,
                    VtValue(value),
                    VtValue(defaultVal),
                    !ignoreDefault,
                    attrs);
            } break;
            default:
                // TODO: Add more types if necessary
                TF_DEBUG(MAYAHYDRALIB_GET_EXTENSION_ATTRS)
                    .Msg(
                        "Not handled custom attribute: name=%s, type=%d\n",
                        attrFn.name().asChar(),
                        attrObj.apiType());
                break;
            }
    }
}
} // namespace MAYAHYDRA_NS_DEF
