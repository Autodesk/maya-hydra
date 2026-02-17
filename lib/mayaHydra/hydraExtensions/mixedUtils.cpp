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

// Store value only when it differs from the default.
template<typename T> 
void UpdateAttrs(const char* attrName, const T& val, const T& defaultVal, VtDictionary& attrs)
{
    if (defaultVal != val) {
        attrs[attrName] = VtValue(val);
    } else {
        attrs.erase(attrName);
    }
}

// Update attribute dictionary with optional default comparison.
void UpdateAttrsValue(
    const char* attrName,
    const VtValue& val,
    const VtValue& defaultVal,
    const bool hasDefault,
    VtDictionary& attrs)
{
    if (hasDefault && (defaultVal == val)) {
        attrs.erase(attrName);
    } else {
        attrs[attrName] = val;
    }
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

// Extract array data from an MObject if possible.
template <typename ArrayType, typename FnDataType>
ArrayType GetArrayFromObject(const MObject& obj)
{
    ArrayType array;
    if (!obj.isNull()) {
        MStatus status;
        FnDataType data(obj, &status);
        if (status) {
            array = data.array();
        }
    }
    return array;
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
        return true;
    }
    case MFnNumericData::kShort: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asShort());
        return true;
    }
    case MFnNumericData::kInt: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asInt());
        return true;
    }
    case MFnNumericData::kInt64: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asInt64());
        return true;
    }
    case MFnNumericData::kFloat: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asFloat());
        return true;
    }
    case MFnNumericData::kDouble: {
        MStatus     handleStatus;
        MDataHandle handle = plug.asMDataHandle(&handleStatus);
        if (!handleStatus) {
            return false;
        }
        outValue = VtValue(handle.asDouble());
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

// Populate a dictionary with extension/dynamic attribute values.
void GetExtensionAttributesFromNode(
    const MObject& node, PXR_NS::VtDictionary& attrs)
{
    MStatus           status;
    MFnDependencyNode nodeFn(node, &status);
    if (ARCH_UNLIKELY(!status)) {
        return;
    }

    for (size_t i = 0; i < nodeFn.attributeCount(); i++) {
        MObject      attrObj = nodeFn.attribute(i);
        MFnAttribute attrFn(attrObj);
        auto         attrName = attrFn.name().asChar();
        MPlug        attrPlug(node, attrObj);
        if (attrPlug.isChild())
            continue;

        if (attrFn.isExtension() || attrFn.isDynamic()) {
            const bool ignoreDefault = attrFn.isDynamic();
            switch (attrObj.apiType()) {
            case MFn::kEnumAttribute: {
                MFnEnumAttribute enumAttr(attrObj);
                short            value = attrPlug.asShort();
                short            defaultVal = 0;
                enumAttr.getDefault(defaultVal);
                UpdateAttrs<short>(attrName, value, defaultVal, attrs);
            } break;
            case MFn::kTypedAttribute: {
                MFnTypedAttribute typeAttr(attrObj);
                switch (typeAttr.attrType()) {
                case MFnData::kString: {
                    MString value = attrPlug.asString();
                    MObject defaultValObj;
                    typeAttr.getDefault(defaultValObj);
                    MFnStringData defaultStringData(defaultValObj);
                    MString       defaultVal = defaultStringData.string();
                    UpdateAttrs<MString>(attrName, value, defaultVal, attrs);
                } break;
                case MFnData::kStringArray: {
                    const auto valueArray
                        = GetArrayFromObject<MStringArray, MFnStringArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MStringArray, MFnStringArrayData>(defaultObj);

                    UpdateAttrs<VtStringArray>(
                        attrName,
                        ToVtStringArray(valueArray),
                        ToVtStringArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kIntArray: {
                    const auto valueArray
                        = GetArrayFromObject<MIntArray, MFnIntArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MIntArray, MFnIntArrayData>(defaultObj);

                    UpdateAttrs<VtIntArray>(
                        attrName,
                        ToVtIntArray(valueArray),
                        ToVtIntArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kFloatArray: {
                    const auto valueArray
                        = GetArrayFromObject<MFloatArray, MFnFloatArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MFloatArray, MFnFloatArrayData>(defaultObj);

                    UpdateAttrs<VtFloatArray>(
                        attrName,
                        ToVtFloatArray(valueArray),
                        ToVtFloatArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kDoubleArray: {
                    const auto valueArray
                        = GetArrayFromObject<MDoubleArray, MFnDoubleArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MDoubleArray, MFnDoubleArrayData>(defaultObj);

                    UpdateAttrs<VtDoubleArray>(
                        attrName,
                        ToVtDoubleArray(valueArray),
                        ToVtDoubleArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kVectorArray: {
                    const auto valueArray
                        = GetArrayFromObject<MVectorArray, MFnVectorArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MVectorArray, MFnVectorArrayData>(defaultObj);

                    UpdateAttrs<VtArray<GfVec3d>>(
                        attrName,
                        ToVtVec3dArray(valueArray),
                        ToVtVec3dArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kPointArray: {
                    const auto valueArray
                        = GetArrayFromObject<MPointArray, MFnPointArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MPointArray, MFnPointArrayData>(defaultObj);

                    UpdateAttrs<VtArray<GfVec4d>>(
                        attrName,
                        ToVtVec4dArray(valueArray),
                        ToVtVec4dArray(defaultArray),
                        attrs);
                } break;
                case MFnData::kMatrix: {
                    MObject dataObj = attrPlug.asMObject();
                    MStatus matrixStatus;
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
                    UpdateAttrs<GfMatrix4d>(attrName, value, defaultVal, attrs);
                } break;
                case MFnData::kMatrixArray: {
                    const auto valueArray
                        = GetArrayFromObject<MMatrixArray, MFnMatrixArrayData>(attrPlug.asMObject());

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    const auto defaultArray
                        = GetArrayFromObject<MMatrixArray, MFnMatrixArrayData>(defaultObj);

                    UpdateAttrs<VtArray<GfMatrix4d>>(
                        attrName,
                        ToVtMatrix4dArray(valueArray),
                        ToVtMatrix4dArray(defaultArray),
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

                    MObject defaultObj;
                    typeAttr.getDefault(defaultObj);
                    VtValue defaultVal;
                    bool    hasDefault = supportsDefault && ExtractNumericDataFromObject(defaultObj, defaultVal);

                    UpdateAttrsValue(attrName, value, defaultVal, hasDefault, attrs);
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
                const unsigned int childCount = attrPlug.numChildren();
                if (childCount < 2) {
                    break;
                }

                MStatus numericStatus;
                MFnNumericAttribute firstChild(attrPlug.child(0).attribute(), &numericStatus);
                if (!numericStatus) {
                    break;
                }
                const auto unitType = firstChild.unitType();
                for (unsigned int i = 1; i < childCount; ++i) {
                    MFnNumericAttribute childAttr(attrPlug.child(i).attribute(), &numericStatus);
                    if (!numericStatus || childAttr.unitType() != unitType) {
                        break;
                    }
                }
                if (!numericStatus) {
                    break;
                }

                switch (unitType) {
                case MFnNumericData::kShort:
                case MFnNumericData::kInt: {
                    const bool isShort = (unitType == MFnNumericData::kShort);
                    if (childCount == 2) {
                        int valueX = isShort ? attrPlug.child(0).asShort() : attrPlug.child(0).asInt();
                        int valueY = isShort ? attrPlug.child(1).asShort() : attrPlug.child(1).asInt();
                        int defaultX = 0;
                        int defaultY = 0;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec2i(valueX, valueY)),
                            VtValue(GfVec2i(defaultX, defaultY)),
                            !ignoreDefault,
                            attrs);
                    } else if (childCount == 3) {
                        int valueX = isShort ? attrPlug.child(0).asShort() : attrPlug.child(0).asInt();
                        int valueY = isShort ? attrPlug.child(1).asShort() : attrPlug.child(1).asInt();
                        int valueZ = isShort ? attrPlug.child(2).asShort() : attrPlug.child(2).asInt();
                        int defaultX = 0;
                        int defaultY = 0;
                        int defaultZ = 0;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        MFnNumericAttribute childAttrZ(attrPlug.child(2).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrZ.getDefault(defaultZ);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec3i(valueX, valueY, valueZ)),
                            VtValue(GfVec3i(defaultX, defaultY, defaultZ)),
                            !ignoreDefault,
                            attrs);
                    }
                } break;
                case MFnNumericData::kFloat: {
                    if (childCount == 2) {
                        float valueX = attrPlug.child(0).asFloat();
                        float valueY = attrPlug.child(1).asFloat();
                        float defaultX = 0.0f;
                        float defaultY = 0.0f;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec2f(valueX, valueY)),
                            VtValue(GfVec2f(defaultX, defaultY)),
                            !ignoreDefault,
                            attrs);
                    } else if (childCount == 3) {
                        float valueX = attrPlug.child(0).asFloat();
                        float valueY = attrPlug.child(1).asFloat();
                        float valueZ = attrPlug.child(2).asFloat();
                        float defaultX = 0.0f;
                        float defaultY = 0.0f;
                        float defaultZ = 0.0f;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        MFnNumericAttribute childAttrZ(attrPlug.child(2).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrZ.getDefault(defaultZ);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec3f(valueX, valueY, valueZ)),
                            VtValue(GfVec3f(defaultX, defaultY, defaultZ)),
                            !ignoreDefault,
                            attrs);
                    }
                } break;
                case MFnNumericData::kDouble: {
                    if (childCount == 2) {
                        double valueX = attrPlug.child(0).asDouble();
                        double valueY = attrPlug.child(1).asDouble();
                        double defaultX = 0.0;
                        double defaultY = 0.0;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec2d(valueX, valueY)),
                            VtValue(GfVec2d(defaultX, defaultY)),
                            !ignoreDefault,
                            attrs);
                    } else if (childCount == 3) {
                        double valueX = attrPlug.child(0).asDouble();
                        double valueY = attrPlug.child(1).asDouble();
                        double valueZ = attrPlug.child(2).asDouble();
                        double defaultX = 0.0;
                        double defaultY = 0.0;
                        double defaultZ = 0.0;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        MFnNumericAttribute childAttrZ(attrPlug.child(2).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrZ.getDefault(defaultZ);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec3d(valueX, valueY, valueZ)),
                            VtValue(GfVec3d(defaultX, defaultY, defaultZ)),
                            !ignoreDefault,
                            attrs);
                    } else if (childCount == 4) {
                        double valueX = attrPlug.child(0).asDouble();
                        double valueY = attrPlug.child(1).asDouble();
                        double valueZ = attrPlug.child(2).asDouble();
                        double valueW = attrPlug.child(3).asDouble();
                        double defaultX = 0.0;
                        double defaultY = 0.0;
                        double defaultZ = 0.0;
                        double defaultW = 0.0;
                        MFnNumericAttribute childAttrX(attrPlug.child(0).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrX.getDefault(defaultX);
                        }
                        MFnNumericAttribute childAttrY(attrPlug.child(1).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrY.getDefault(defaultY);
                        }
                        MFnNumericAttribute childAttrZ(attrPlug.child(2).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrZ.getDefault(defaultZ);
                        }
                        MFnNumericAttribute childAttrW(attrPlug.child(3).attribute(), &numericStatus);
                        if (numericStatus) {
                            childAttrW.getDefault(defaultW);
                        }
                        UpdateAttrsValue(
                            attrName,
                            VtValue(GfVec4d(valueX, valueY, valueZ, valueW)),
                            VtValue(GfVec4d(defaultX, defaultY, defaultZ, defaultW)),
                            !ignoreDefault,
                            attrs);
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
                    bool defaultVal = false;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<bool>(attrName, value, defaultVal, attrs);
                } break;
                case MFnNumericData::kByte:
                case MFnNumericData::kChar: {
                    auto value = static_cast<int>(attrPlug.asChar());
                    char defaultVal = 0;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<int>(attrName, value, static_cast<int>(defaultVal), attrs);
                } break;
                case MFnNumericData::kShort: {
                    auto  value = attrPlug.asShort();
                    int defaultVal = 0;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<short>(
                        attrName,
                        value,
                        static_cast<short>(defaultVal),
                        attrs);
                } break;
                case MFnNumericData::kInt: {
                    auto value = attrPlug.asInt();
                    int  defaultVal = 0;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<int>(attrName, value, defaultVal, attrs);
                } break;
                case MFnNumericData::kFloat: {
                    auto  value = attrPlug.asFloat();
                    float defaultVal = 0.0f;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<float>(attrName, value, defaultVal, attrs);
                } break;
                case MFnNumericData::kDouble: {
                    auto   value = attrPlug.asDouble();
                    double defaultVal = 0.0;
                    numericAttr.getDefault(defaultVal);
                    UpdateAttrs<double>(attrName, value, defaultVal, attrs);
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
                    short x = 0;
                    short y = 0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 2) {
                        x = attrPlug.child(0).asShort();
                        y = attrPlug.child(1).asShort();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asShort();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asShort();
                            found1 = true;
                        }
                        gotChild = found0 && found1;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData2Short(x, y)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData2Short(x, y)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    int defaultX = 0;
                    int defaultY = 0;
                    numericAttr.getDefault(defaultX, defaultY);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec2i(static_cast<int>(x), static_cast<int>(y))),
                        VtValue(GfVec2i(defaultX, defaultY)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k3Short: {
                    short x = 0;
                    short y = 0;
                    short z = 0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 3) {
                        x = attrPlug.child(0).asShort();
                        y = attrPlug.child(1).asShort();
                        z = attrPlug.child(2).asShort();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        bool          found2 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asShort();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asShort();
                            found1 = true;
                        }
                        MPlug child2 = FindPlugWithFallback(nodeFn, baseName + "2", childStatus);
                        if (childStatus) {
                            z = child2.asShort();
                            found2 = true;
                        }
                        gotChild = found0 && found1 && found2;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData3Short(x, y, z)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData3Short(x, y, z)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    int defaultX = 0;
                    int defaultY = 0;
                    int defaultZ = 0;
                    numericAttr.getDefault(defaultX, defaultY, defaultZ);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec3i(static_cast<int>(x), static_cast<int>(y), static_cast<int>(z))),
                        VtValue(GfVec3i(defaultX, defaultY, defaultZ)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k2Int: {
                    int x = 0;
                    int y = 0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 2) {
                        x = attrPlug.child(0).asInt();
                        y = attrPlug.child(1).asInt();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asInt();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asInt();
                            found1 = true;
                        }
                        gotChild = found0 && found1;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData2Int(x, y)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData2Int(x, y)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    int defaultX = 0;
                    int defaultY = 0;
                    numericAttr.getDefault(defaultX, defaultY);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec2i(x, y)),
                        VtValue(GfVec2i(defaultX, defaultY)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k3Int: {
                    int x = 0;
                    int y = 0;
                    int z = 0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 3) {
                        x = attrPlug.child(0).asInt();
                        y = attrPlug.child(1).asInt();
                        z = attrPlug.child(2).asInt();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        bool          found2 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asInt();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asInt();
                            found1 = true;
                        }
                        MPlug child2 = FindPlugWithFallback(nodeFn, baseName + "2", childStatus);
                        if (childStatus) {
                            z = child2.asInt();
                            found2 = true;
                        }
                        gotChild = found0 && found1 && found2;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData3Int(x, y, z)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData3Int(x, y, z)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    int defaultX = 0;
                    int defaultY = 0;
                    int defaultZ = 0;
                    numericAttr.getDefault(defaultX, defaultY, defaultZ);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec3i(x, y, z)),
                        VtValue(GfVec3i(defaultX, defaultY, defaultZ)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k2Float: {
                    float x = 0.0f;
                    float y = 0.0f;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 2) {
                        x = attrPlug.child(0).asFloat();
                        y = attrPlug.child(1).asFloat();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asFloat();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asFloat();
                            found1 = true;
                        }
                        gotChild = found0 && found1;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData2Float(x, y)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData2Float(x, y)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    float defaultX = 0.0f;
                    float defaultY = 0.0f;
                    numericAttr.getDefault(defaultX, defaultY);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec2f(x, y)),
                        VtValue(GfVec2f(defaultX, defaultY)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k3Float: {
                    float x = 0.0f;
                    float y = 0.0f;
                    float z = 0.0f;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 3) {
                        x = attrPlug.child(0).asFloat();
                        y = attrPlug.child(1).asFloat();
                        z = attrPlug.child(2).asFloat();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        bool          found2 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asFloat();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asFloat();
                            found1 = true;
                        }
                        MPlug child2 = FindPlugWithFallback(nodeFn, baseName + "2", childStatus);
                        if (childStatus) {
                            z = child2.asFloat();
                            found2 = true;
                        }
                        gotChild = found0 && found1 && found2;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData3Float(x, y, z)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData3Float(x, y, z)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    float defaultX = 0.0f;
                    float defaultY = 0.0f;
                    float defaultZ = 0.0f;
                    numericAttr.getDefault(defaultX, defaultY, defaultZ);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec3f(x, y, z)),
                        VtValue(GfVec3f(defaultX, defaultY, defaultZ)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k2Double: {
                    double x = 0.0;
                    double y = 0.0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 2) {
                        x = attrPlug.child(0).asDouble();
                        y = attrPlug.child(1).asDouble();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asDouble();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asDouble();
                            found1 = true;
                        }
                        gotChild = found0 && found1;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData2Double(x, y)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData2Double(x, y)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    double defaultX = 0.0;
                    double defaultY = 0.0;
                    numericAttr.getDefault(defaultX, defaultY);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec2d(x, y)),
                        VtValue(GfVec2d(defaultX, defaultY)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k3Double: {
                    double x = 0.0;
                    double y = 0.0;
                    double z = 0.0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 3) {
                        x = attrPlug.child(0).asDouble();
                        y = attrPlug.child(1).asDouble();
                        z = attrPlug.child(2).asDouble();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          found0 = false;
                        bool          found1 = false;
                        bool          found2 = false;
                        MPlug         child0 = FindPlugWithFallback(nodeFn, baseName + "0", childStatus);
                        if (childStatus) {
                            x = child0.asDouble();
                            found0 = true;
                        }
                        MPlug child1 = FindPlugWithFallback(nodeFn, baseName + "1", childStatus);
                        if (childStatus) {
                            y = child1.asDouble();
                            found1 = true;
                        }
                        MPlug child2 = FindPlugWithFallback(nodeFn, baseName + "2", childStatus);
                        if (childStatus) {
                            z = child2.asDouble();
                            found2 = true;
                        }
                        gotChild = found0 && found1 && found2;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData3Double(x, y, z)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData3Double(x, y, z)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    double defaultX = 0.0;
                    double defaultY = 0.0;
                    double defaultZ = 0.0;
                    numericAttr.getDefault(defaultX, defaultY, defaultZ);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec3d(x, y, z)),
                        VtValue(GfVec3d(defaultX, defaultY, defaultZ)),
                        !ignoreDefault,
                        attrs);
                } break;
                case MFnNumericData::k4Double: {
                    double x = 0.0;
                    double y = 0.0;
                    double z = 0.0;
                    double w = 0.0;
                    bool gotValue = false;
                    bool gotChild = false;
                    if (attrPlug.numChildren() >= 4) {
                        x = attrPlug.child(0).asDouble();
                        y = attrPlug.child(1).asDouble();
                        z = attrPlug.child(2).asDouble();
                        w = attrPlug.child(3).asDouble();
                        gotChild = true;
                    }
                    if (!gotChild) {
                        const MString baseName = attrFn.name();
                        MStatus       childStatus;
                        bool          foundX = false;
                        bool          foundY = false;
                        bool          foundZ = false;
                        bool          foundW = false;
                        MPlug         childX = FindPlugWithFallback(nodeFn, baseName + "X", childStatus);
                        if (childStatus) {
                            x = childX.asDouble();
                            foundX = true;
                        }
                        MPlug childY = FindPlugWithFallback(nodeFn, baseName + "Y", childStatus);
                        if (childStatus) {
                            y = childY.asDouble();
                            foundY = true;
                        }
                        MPlug childZ = FindPlugWithFallback(nodeFn, baseName + "Z", childStatus);
                        if (childStatus) {
                            z = childZ.asDouble();
                            foundZ = true;
                        }
                        MPlug childW = FindPlugWithFallback(nodeFn, baseName + "W", childStatus);
                        if (childStatus) {
                            w = childW.asDouble();
                            foundW = true;
                        }
                        gotChild = foundX && foundY && foundZ && foundW;
                    }
                    if (gotChild) {
                        gotValue = true;
                    }
                    if (!gotValue) {
                        MStatus     handleStatus;
                        MDataHandle handle = attrPlug.asMDataHandle(&handleStatus);
                        if (handleStatus) {
                            MObject       dataObj = handle.data();
                            MFnNumericData numericData(dataObj, &handleStatus);
                            if (handleStatus && numericData.getData4Double(x, y, z, w)) {
                                gotValue = true;
                            }
                        }
                    }
                    if (!gotValue) {
                        MStatus       dataStatus;
                        MObject       dataObj = attrPlug.asMObject();
                        MFnNumericData numericData(dataObj, &dataStatus);
                        if (dataStatus && numericData.getData4Double(x, y, z, w)) {
                            gotValue = true;
                        }
                    }
                    if (!gotValue) {
                        break;
                    }
                    double defaultX = 0.0;
                    double defaultY = 0.0;
                    double defaultZ = 0.0;
                    double defaultW = 0.0;
                    numericAttr.getDefault(defaultX, defaultY, defaultZ, defaultW);
                    UpdateAttrsValue(
                        attrName,
                        VtValue(GfVec4d(x, y, z, w)),
                        VtValue(GfVec4d(defaultX, defaultY, defaultZ, defaultW)),
                        !ignoreDefault,
                        attrs);
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

                UpdateAttrs<GfMatrix4d>(attrName, value, defaultVal, attrs);
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
}
} // namespace MAYAHYDRA_NS_DEF
