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

#include <mayaHydraLib/adapters/mayaAttrs.h>

#include <maya/MDagPath.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>

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

MStatus GetObjectsFromNodeNames(const MStringArray& nodeNames, MObjectArray& outObjects)
{
    const unsigned int numObjects = outObjects.length();
    if (nodeNames.length() != numObjects) {
        return MStatus::kInvalidParameter;
    }

    for (auto& obj : outObjects) {
        obj = MObject::kNullObj;
    }

    MStatus        status;
    MSelectionList sList;
    for (const auto& nodeName : nodeNames) {
        status = sList.add(nodeName);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    for (unsigned int i = 0; i < numObjects; ++i) {
        status = sList.getDependNode(i, outObjects[i]);
        CHECK_MSTATUS_AND_RETURN_IT(status);
    }

    return MS::kSuccess;
}

bool IsDagPathAnArnoldSkyDomeLight(const MDagPath& dagPath)
{
    static const MString _aiSkyDomeLight("aiSkyDomeLight");
    return IsDagPathOfGivenType(dagPath, _aiSkyDomeLight);
}

bool IsDagPathAnArnoldAreaLight(const MDagPath& dagPath)
{
    static const MString _aiAreaLight("aiAreaLight");
    return IsDagPathOfGivenType(dagPath, _aiAreaLight);
}

bool IsDagPathALight(const MDagPath& dagPath)
{
    static const MString _lightString("Light");

    if (!dagPath.isValid())
        return false;
    auto shapeDagPath = dagPath;
    shapeDagPath.extendToShape();
    const MString typeName = MFnDependencyNode(shapeDagPath.node()).typeName();
    return (typeName.indexW(_lightString) != -1); // Does the typename contains "Light"
}

std::string GetDomeLightTexture(const MFnDependencyNode& lightNode)
{
    // Be aware that dome lights in HdStorm always need a texture to work correctly,
    // the color is not used if no texture is present.
    const auto plug = lightNode.findPlug("color", true);
    MPlugArray conns;
    plug.connectedTo(conns, true, false);
    const bool _colorIsConnected = (conns.length() > 0);
    if (!_colorIsConnected) {
        return "";
    }

    MStatus              status;
    MFnDependencyNode    file(conns[0].node(), &status);
    static const MString fileString("file");
    if (!status || (file.typeName() != fileString)) {
        return "";
    }

    const char* fileTextureName
        = file.findPlug(PXR_NS::MayaAttrs::file::fileTextureName, true).asString().asChar();

    return fileTextureName;
}

bool IsDagPathOfGivenType(const MDagPath& dagPath, const MString& type)
{
    if (!dagPath.isValid()) {
        return false;
    }
    auto shapeDagPath = dagPath;
    shapeDagPath.extendToShape();
    return type == MFnDependencyNode(shapeDagPath.node()).typeName();
}

} // namespace MAYAHYDRA_NS_DEF