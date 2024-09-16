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

#include <maya/MDagPath.h>
#include <maya/MGlobal.h>
#include <maya/MHWGeometry.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MRenderUtil.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

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

SdfPath DagPathToSdfPath(
    const MDagPath& dagPath,
    const bool      mergeTransformAndShape,
    const bool      stripNamespaces)
{
    std::string name = dagPath.fullPathName().asChar();
    if ( name.empty() ) {
        MFnDependencyNode dep(dagPath.node());
        TF_WARN("Empty fullpath name for DAG object : %s of type : %s", dep.name().asChar(), dep.typeName().asChar());
        return SdfPath();
    }
    SanitizeNameForSdfPath(name, stripNamespaces);
    SdfPath usdPath(name);

    if (mergeTransformAndShape && IsShape(dagPath)) {
        usdPath = usdPath.GetParentPath();
    }

    return usdPath;
}

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

PXR_NS::GfVec4f getPreferencesColor(const PXR_NS::TfToken& token)
{
    PXR_NS::GfVec4f color;
    Fvp::ColorPreferences::getInstance().getColor(token, color);
    return color;
}

PXR_NS::TfToken GetGeomSubsetsPickMode()
{
    static const MString kOptionVarName(MayaHydraPickOptionVars->GeomSubsetsPickMode.GetText());

    if (MGlobal::optionVarExists(kOptionVarName)) {
        return TfToken(MGlobal::optionVarStringValue(kOptionVarName).asChar());
    }

    return GeomSubsetsPickModeTokens->None;
}

} // namespace MAYAHYDRA_NS_DEF
