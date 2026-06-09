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

#include <pxr/base/tf/diagnostic.h>

#include <maya/MDagPath.h>
#include <maya/MFnComponent.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MObjectArray.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MSelectionList.h>
#include <maya/MStringArray.h>

PXR_NAMESPACE_USING_DIRECTIVE

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

MPlug GetTopPlug(const MPlug& plug)
{
    MPlug topPlug = plug;
    while (topPlug.isChild()) {
        topPlug = topPlug.parent();
    }
    if (topPlug.isElement()) {
        topPlug = topPlug.array();
    }
    return topPlug;
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
    MPlug ufeRuntimePlug = dagNode.findPlug(ufeRuntimeAttributeName, false);
    MStatus ufePlugSearchStatus = ufeRuntimePlug.isNull() ? MS::kFailure : MS::kSuccess;
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

bool IsDagPathACamera(const MDagPath& dagPath)
{
    if (!dagPath.isValid())
        return false;
    auto shapeDagPath = dagPath;
    shapeDagPath.extendToShape();
    return shapeDagPath.hasFn(MFn::kCamera);
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

// Collect every shading-group assignment on a shape.
//
// What: appends one ShadingAssignment per kShadingEngine connected to dagPath,
// pairing each shading engine with the component (set of faces) it is assigned
// to. The component is null for a whole-object assignment.
// How: Maya exposes per-instance assignments through
// MFnDagNode::getConnectedSetsAndMembers, which returns parallel arrays of sets
// and their member components. We keep only the shading-engine sets. Callers use
// this to build Hydra geomSubsets for per-face (multi-material) meshes.
void GetAllShadingAssignments(const MDagPath& dagPath, std::vector<ShadingAssignment>& out)
{
    if (!dagPath.isValid())
        return;

    MFnDagNode dagNode(dagPath.node());
    MObjectArray sets, comps;
    dagNode.getConnectedSetsAndMembers(dagPath.instanceNumber(), sets, comps, /*renderableSetsOnly=*/true);
    if (sets.length() != comps.length()) {
        TF_WARN("GetAllShadingAssignments: sets/comps length mismatch on %s", dagPath.fullPathName().asChar());
        return;
    }
    for (uint32_t i = 0; i < sets.length(); ++i) {
        if (sets[i].apiType() == MFn::kShadingEngine) {
            out.push_back({ comps[i], sets[i] });
        }
    }
}

// Find the single shading engine that applies to a shape (or to one of its
// components).
//
// What: returns the kShadingEngine MObject assigned to dagPath. When
// shadingComp is non-null, only the shading engine whose member component
// matches shadingComp is returned (per-face shading, e.g. one render item of a
// multi-material mesh); when it is null the first shading engine found is
// returned (whole-object assignment). Returns kNullObj when nothing is assigned.
// How: same getConnectedSetsAndMembers enumeration as GetAllShadingAssignments,
// but short-circuits on the first matching shading engine. MFnComponent::isEqual
// is used to match the requested component against each set's member component.
MObject FindShadingEngine(const MDagPath& dagPath, const MObject& shadingComp)
{
    if (!dagPath.isValid())
        return MObject::kNullObj;

    MFnDagNode   dagNode(dagPath.node());
    MObjectArray sets, comps;
    dagNode.getConnectedSetsAndMembers(dagPath.instanceNumber(), sets, comps, /*renderableSetsOnly=*/true);
    if (sets.length() != comps.length()) {
        TF_WARN("FindShadingEngine: sets/comps length mismatch on %s", dagPath.fullPathName().asChar());
        return MObject::kNullObj;
    }
    for (uint32_t i = 0; i < sets.length(); ++i) {
        if (sets[i].apiType() == MFn::kShadingEngine) {
            MObject comp = comps[i];
            MObject shadingCompCopy = shadingComp;
            if (shadingCompCopy.isNull() || comp.isNull() || MFnComponent(comp).isEqual(shadingCompCopy))
                return sets[i];
        }
    }
    return MObject::kNullObj;
}

} // namespace MAYAHYDRA_NS_DEF
