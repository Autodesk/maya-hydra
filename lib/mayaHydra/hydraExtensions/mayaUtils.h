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
#include <maya/MPlug.h>

#include <string>

namespace MAYAHYDRA_NS_DEF {

// Names of color tables for indexed colors
const std::string kActiveColorTableName = "active";
const std::string kDormantColorTableName = "dormant";

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
MStatus GetObjectsFromNodeNames(const MStringArray& nodeNames, MObjectArray & outObjects);

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
 * @brief Is it a maya node transform attribute ?
 *
 * @param[in] attrName is an attribute's name.
 *
 * @return true if this is a maya transform attribute name, false otherwise.
 */
MAYAHYDRALIB_API 
bool IsAMayaTransformAttributeName(const MString& attrName);

/**
 * @brief Is it a maya node visibility attribute ?
 *
 * If so then we fill the outVal with the visibility value
 * 
 * @param[in] plug is a MPlug from an attribute.
 * @param[out] outVal will contain true or false if that was a maya visibility attribute 
 *
 * @return true if this is a maya visibility attribute name, false otherwise.
 */

//Is it a maya node visibility attribute ? 
bool IsAMayaVisibilityAttribute(const MPlug& plug, bool& outVal);

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
    MStatus plugStatus;
    MPlug   plug = dependencyNode.findPlug(attrName.c_str(), true, &plugStatus);
    if (!plugStatus) {
        return false;
    }
    return plug.setValue(newValue);
}

/**
 * @brief Get the shading group MObject from a shader MObject.
 *
 * @param[in] shader is the MObject of the shader
 *
 * @return the MObject of the shading group
 */
MObject GetShadingGroupFromShader(const MObject& shader);

/**
 * @brief Get if this MDagPath is an Arnold sky dome light.
 *
 * @param[in] dagPath is a MDagPath
 *
 * @return true if the object is a sky dome light, false otherwise
 */
bool IsDagPathAnArnoldSkyDomeLight(const MDagPath& dagPath);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MAYA_UTILS_H
