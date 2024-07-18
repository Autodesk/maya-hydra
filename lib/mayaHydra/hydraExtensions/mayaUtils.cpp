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

#include "mayaUtils.h"

#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MGlobal.h>
#include <maya/MMatrix.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MObjectArray.h>
#include <maya/MFnAttribute.h>

namespace
{
    //To compute the size of an array automatically
    template<typename T, std::size_t N>
    constexpr std::size_t arraySize(T(&)[N]) noexcept
    {
        return N;
    }

    ///Is an array of strings that are all maya transform attributes names
    const char* kMayaTransformAttributesStrings[] = {"translateX", "translateY", "translateZ",
                                                "rotatePivotTranslateX", "rotatePivotTranslateY", "rotatePivotTranslateZ",
                                                "rotatePivotX", "rotatePivotY", "rotatePivotZ", 
                                                "rotateX", "rotateY","rotateZ",
                                                "rotateAxisX", "rotateAxisY", "rotateAxisZ",
                                                "scalePivotTranslateX", "scalePivotTranslateY", "scalePivotTranslateZ",
                                                "scalePivotX", "scalePivotY", "scalePivotZ",
                                                "shearXY", "shearXZ", "shearYZ",
                                                "scaleX", "scaleY", "scaleZ",
                                                "worldMatrix",
                                                "localPositionX", "localPositionY", "localPosition",
                                                "translate", "rotate", "scale"
                                                };
    //Convert from const char* [] to MStringArray
    const MStringArray transformAttrNames(kMayaTransformAttributesStrings, arraySize(kMayaTransformAttributesStrings));

    //For visibility attributes
    const char* visibilityNames[] = {"visibility"};
    
    //For visibility attributes
    const MStringArray visibilityAttrNames = MStringArray(visibilityNames, arraySize(visibilityNames));
}

namespace MAYAHYDRA_NS_DEF {


MStatus GetDagPathFromNodeName(const MString& nodeName, MDagPath& outDagPath)
{
    MSelectionList selectionList;
    MStatus        status = selectionList.add(nodeName);
    if (status) {
        status = selectionList.getDagPath(0, outDagPath);
    }
    return status;
}

MStatus GetDependNodeFromNodeName(const MString& nodeName, MObject& outDependNode)
{
    MSelectionList selectionList;
    MStatus        status = selectionList.add(nodeName);
    if (status) {
        status = selectionList.getDependNode(0, outDependNode);
    }
    return status;
}

MStatus GetMayaMatrixFromDagPath(const MDagPath& dagPath, MMatrix& outMatrix)
{
    MStatus status;
    outMatrix = dagPath.inclusiveMatrix(&status);
    return status;
}

bool IsUfeItemFromMayaUsd(const MDagPath& dagPath, MStatus* returnStatus)
{
    static const MString ufeRuntimeAttributeName = "ufeRuntime";
    static const MString mayaUsdUfeRuntimeName = "USD";

    MFnDagNode dagNode(dagPath);
    MStatus    ufePlugSearchStatus;
    MPlug ufeRuntimePlug = dagNode.findPlug(ufeRuntimeAttributeName, false, &ufePlugSearchStatus);
    if (returnStatus) {
        *returnStatus = ufePlugSearchStatus;
    }
    return ufePlugSearchStatus && ufeRuntimePlug.asString() == mayaUsdUfeRuntimeName;
}

bool IsUfeItemFromMayaUsd(const MObject& obj, MStatus* returnStatus)
{
    MDagPath dagPath;
    MStatus  dagPathSearchStatus = MDagPath::getAPathTo(obj, dagPath);
    if (!dagPathSearchStatus) {
        if (returnStatus) {
            *returnStatus = dagPathSearchStatus;
        }
        return false;
    }

    return IsUfeItemFromMayaUsd(dagPath, returnStatus);
}

MStatus GetObjectsFromNodeNames(const MStringArray& nodeNames, MObjectArray & outObjects)
{
    const unsigned int numObjects = outObjects.length() ;
    if (nodeNames.length() != numObjects){
        return MStatus::kInvalidParameter;
    }

    for (auto& obj : outObjects){
        obj = MObject::kNullObj;
    }

    MStatus status;
    MSelectionList sList;
    for (const auto& nodeName : nodeNames){
        status = sList.add(nodeName);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    for (unsigned int i=0;i<numObjects;++i){
        status = sList.getDependNode(i, outObjects[i]);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }
    
    return MS::kSuccess;
}

bool IsAMayaTransformAttributeName(const MString& attrName)
{ 
    return (-1 != transformAttrNames.indexOf(attrName));
}

bool IsAMayaVisibilityAttribute(const MPlug& plug, bool& outVal)
{
    //Get the visibility value from MPlug
    MFnAttribute attr (plug.attribute());
    bool isVisibility = -1 != visibilityAttrNames.indexOf(attr.name());
    if (isVisibility){
        plug.getValue(outVal);
    }
    return isVisibility;
}

MObject GetShadingGroupFromShader(const MObject& shader)
{
    MObject           shadingGroup;
    MFnDependencyNode fn(shader);

    // Get the "outColor" plug of the shader
    MPlug outColorPlug = fn.findPlug("outColor", true);

    // Get the connected plugs
    MPlugArray connectedPlugs;
    outColorPlug.connectedTo(connectedPlugs, false, true);

    // Loop over the connected plugs
    for (unsigned int i = 0; i < connectedPlugs.length(); ++i) {
        MObject node = connectedPlugs[i].node();
        if (node.apiType() == MFn::kShadingEngine) // Check if the node is a shading group
        {
            shadingGroup = node;
            break;
        }
    }

    return shadingGroup;
}

} // namespace MAYAHYDRA_NS_DEF
