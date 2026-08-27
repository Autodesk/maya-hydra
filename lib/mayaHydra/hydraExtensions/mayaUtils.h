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

#ifndef MAYAHYDRALIB_MAYA_UTILS_H
#define MAYAHYDRALIB_MAYA_UTILS_H

#include <mayaHydraLib/api.h>

#include <maya/MApiNamespace.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MTypes.h>

#include <string>
#include <string_view>
#include <vector>

namespace MAYAHYDRA_NS_DEF {

inline constexpr std::string_view kUsdDefaultRenderDescriptionNodeName = "UsdDefaultRenderDescription";

// Names of color tables for indexed colors
const std::string kActiveColorTableName = "active";

// Color names
const std::string kLeadColorName = "lead";
const std::string kPolymeshActiveColorName = "polymeshActive";
const std::string kPolymeshDormantColorName = "polymeshDormant";
const std::string kPolyVertexColorName = "polyVertex";
const std::string kPolyEdgeColorName = "polyEdge";
const std::string kPolyFaceColorName = "polyFace";

/**
 * @brief Retrieve several nodes MObject from their name.
 *
 * @param[in] nodeNames are the names of the nodes whose MObject is wanted.
 * @param[out] outObjects are the MObject of each node retrieved.
 *
 * @return The resulting status of the operation.
 */
MAYAHYDRALIB_API
MStatus GetObjectsFromNodeNames(const MStringArray& nodeNames, MObjectArray& outObjects);

/**
 * @brief Get the DAG path of a node from the Maya scene graph using its name
 *
 * @param[in] nodeName is the name of the node to get the DAG path of.
 * @param[out] outDagPath is the DAG path of the node in the Maya scene graph.
 *
 * @return The resulting status of the operation.
 */
MAYAHYDRALIB_API
MStatus GetDagPathFromNodeName(const MString& nodeName, MDagPath& outDagPath);

/**
 * @brief Get a node from the Maya dependency graph using its name
 *
 * @param[in] nodeName is the name of the node to get.
 * @param[out] outDependNode is the node in the Maya dependency graph.
 *
 * @return The resulting status of the operation.
 */
MAYAHYDRALIB_API
MStatus GetDependNodeFromNodeName(const MString& nodeName, MObject& outDependNode);

/**
 * @brief Return the top-level plug for a child/array element plug.
 *
 * (e.g. aiLookAt[0].child(0) -> aiLookAt)
 */
MAYAHYDRALIB_API
MPlug GetTopPlug(const MPlug& plug);

/**
 * @brief Get the Maya transform matrix of a node from its DAG path
 *
 * The output transform matrix is the resultant ("flattened") matrix from it and
 * its parents' transforms.
 *
 * @param[in] dagPath is the DAG path of the node in the Maya scene graph.
 * @param[out] outMatrix is the Maya transform matrix of the node.
 *
 * @return The resulting status of the operation.
 */
MAYAHYDRALIB_API
MStatus GetMayaMatrixFromDagPath(const MDagPath& dagPath, MMatrix& outMatrix);

/**
 * @brief Determines whether a given DAG path points to a UFE item created by maya-usd
 *
 * UFE stands for Universal Front End : the goal of the Universal Front End is to create a
 * DCC-agnostic component that will allow a DCC to browse and edit data in multiple data models.
 *
 * @param[in] dagPath is the DAG path of the node in the Maya scene graph.
 * @param[out] returnStatus is an optional output variable to return whether the operation was
 * successful. Default value is nullptr (not going to store the result status).
 *
 * @return True if the item pointed to by dagPath is a UFE item created by maya-usd, false
 * otherwise.
 */
MAYAHYDRALIB_API
bool IsUfeItemFromMayaUsd(const MDagPath& dagPath, MStatus* returnStatus = nullptr);

/**
 * @brief Determines whether a given object is a UFE item created by maya-usd
 *
 * UFE stands for Universal Front End : the goal of the Universal Front End is to create a
 * DCC-agnostic component that will allow a DCC to browse and edit data in multiple data models.
 *
 * @param[in] obj is the object representing the DAG node.
 * @param[out] returnStatus is an optional output variable to return whether the operation was
 * successful. Default value is nullptr (not going to store the result status).
 *
 * @return True if the item represented by obj is a UFE item created by maya-usd, false
 * otherwise.
 */
MAYAHYDRALIB_API
bool IsUfeItemFromMayaUsd(const MObject& obj, MStatus* returnStatus = nullptr);

/**
 * @brief Set the value of a DG node attribute.
 *
 * @param[in] node The Maya node for which to modify the attribute
 * @param[in] attrName The attribute name to modify
 * @param[in] newValue The value to set the attribute to
 *
 * @return True if the attribute was successfully modified, false otherwise.
 */
template <typename AttrType>
bool SetNodeAttribute(MObject node, std::string attrName, AttrType newValue)
{
    MStatus           dependencyNodeStatus;
    MFnDependencyNode dependencyNode(node, &dependencyNodeStatus);
    if (!dependencyNodeStatus) {
        return false;
    }
    MPlug plug = dependencyNode.findPlug(attrName.c_str(), true);
    if (plug.isNull()) {
        return false;
    }
    return plug.setValue(newValue);
}

/**
 * @brief Read a double3 numeric attribute from a dependency node.
 *
 * @param[out] outVal The attribute value.
 * @param[in] node The node that owns the attribute.
 * @param[in] attr The MObject of the attribute definition.
 *
 * @return MS::kSuccess when the value was read, MS::kFailure otherwise.
 */
MAYAHYDRALIB_API
MStatus GetDouble3AttributeValue(double3& outVal, const MObject& node, const MObject& attr);

/**
 * @brief True when the plug itself, or, for compound plugs, any of its children is connected.
 *
 * MPlug::isConnected() only reports the queried plug: a keyframe set on a compound
 * attribute (e.g. setKeyframe on a double3 "color") connects anim curves to the
 * individual children (colorR/G/B), not to the compound plug, so isConnected()
 * alone would miss it.
 *
 * @param[in] plug The plug to test.
 *
 * @return True if the plug or one of its children is connected, false otherwise.
 */
MAYAHYDRALIB_API
bool PlugOrChildIsConnected(const MPlug& plug);

/**
 * @brief Get if this MDagPath is an Arnold sky dome light.
 *
 * @param[in] dagPath is a MDagPath
 *
 * @return true if the object is an Arnold sky dome light, false otherwise
 */
bool IsDagPathAnArnoldSkyDomeLight(const MDagPath& dagPath);

/**
 * @brief Get if this MDagPath is an Arnold Area light.
 *
 * @param[in] dagPath is a MDagPath
 *
 * @return true if the object is an Arnold area light, false otherwise
 */
bool IsDagPathAnArnoldAreaLight(const MDagPath& dagPath);

/**
 * @brief Get if this MDagPath is a light.
 *
 * @param[in] dagPath is a MDagPath
 *
 * @return true if the object is a light, false otherwise
 */
bool IsDagPathALight(const MDagPath& dagPath);

/**
 * @brief Get if this MDagPath is a camera.
 *
 * Works whether dagPath points to the camera shape or its parent transform.
 *
 * Checks support for MFn::kCamera, which is more general than the exact
 * type match requirement of IsDagPathOfGivenType().
 *
 * @param[in] dagPath is a MDagPath
 *
 * @return true if the object is a camera, false otherwise
 */
bool IsDagPathACamera(const MDagPath& dagPath);

/**
 * @brief Retrieves the texture file path from a dome light node.
 *
 * This function extracts the texture file path from a Maya dome light node,
 * which is typically used for environment lighting or sky dome lighting.
 * The function is designed to work with Arnold sky dome lights and other
 * dome light types that support texture-based lighting.
 *
 * @param[in] lightNode The Maya dependency node representing the dome light.
 *                      Must be a valid dome light node (e.g., aiSkyDomeLight).
 *
 * @return A string containing the full path to the texture file used by the
 *         dome light. Returns an empty string if:
 *         - The light node is not a valid dome light
 *         - No texture file is assigned to the dome light
 *         - The texture file path cannot be retrieved
 *
 * @note The returned path may be relative or absolute depending on how the
 *       texture was assigned in Maya. It's the caller's responsibility to
 *       resolve relative paths if needed.
 *
 * @see IsDagPathAnArnoldSkyDomeLight() for checking if a DAG path is an Arnold sky dome light
 */
std::string GetDomeLightTexture(const MFnDependencyNode& lightNode);

/**
 * @brief Get if this MDagPath is of the given type.
 *
 * @param[in] dagPath is a MDagPath
 * @param[in] type is a Maya type string
 *
 * @return true if the object is a dag path of the given type, false otherwise
 */
bool IsDagPathOfGivenType(const MDagPath& dagPath, const MString& type);

/**
 * @brief Find the shading engine (kShadingEngine) connected to a DAG path.
 *
 * When shadingComp is non-null, only returns the shading engine whose component
 * matches shadingComp (per-face shading). When shadingComp is null, returns the
 * first shading engine found (whole-object assignment).
 *
 * @param[in] dagPath     The DAG path of the shape.
 * @param[in] shadingComp Optional component for per-face shading matching.
 *
 * @return The shading engine MObject, or MObject::kNullObj if none is found.
 */
MAYAHYDRALIB_API
MObject FindShadingEngine(const MDagPath& dagPath, const MObject& shadingComp = MObject::kNullObj);

/// One entry per shading group connected to a DAG path.
/// component is MObject::kNullObj for whole-object assignments.
struct ShadingAssignment {
    MObject component;
    MObject shadingEngine;
};

/// Return all shading assignments for dagPath.
/// Uses getConnectedSetsAndMembers so per-face assignments are included.
MAYAHYDRALIB_API
void GetAllShadingAssignments(const MDagPath& dagPath, std::vector<ShadingAssignment>& out);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MAYA_UTILS_H
