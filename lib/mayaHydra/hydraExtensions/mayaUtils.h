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
#include <mayaHydraLib/mayaHydra.h>

#include <maya/MApiNamespace.h>
#include <maya/MFn.h>
#include <string>

namespace MAYAHYDRA_NS_DEF {

// Names of color tables for indexed colors
const std::string kActiveColorTableName = "active";
const std::string kDormantColorTableName = "dormant";

// Color names
const std::string kLeadColorName = "lead";
const std::string kPolymeshActiveColorName = "polymeshActive";
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
 * @param[out] outVal wilml contain true or false if that was a maya visibility attribute 
 *
 * @return true if this is a maya visibility attribute name, false otherwise.
 */
MAYAHYDRALIB_API 
bool IsAMayaVisibilityAttribute(const MPlug& plug, bool& outVal);

/**
 * @brief Get the index of the normals vertex buffer in the geometry
 *
 * @param[in] geom is the geometry to get the normals vertex buffer index from.
 *
 * @return The index of the normals vertex buffer in the geometry or -1 if not found
 */
MAYAHYDRALIB_API 
int GetNormalsVertexBufferIndex(const MGeometry& geom);

/**
 * @brief Get a connected node from its type. The node is searched from a Dependency node connections (in their destination MPlug, not source)
 *
 * @param[in] node is the MFnDependencyNode to look for in its destination connection
 * 
 * @param[out] outNode is the MObjectof the node we were looking for
 * 
 * @param[in] nodeType is the type of the node we were looking for
 * 
 * @return true if the node was found, false otherwise
 */
bool GetTypedNodeFromDestinationConnections(MFnDependencyNode& node, MObject& outNode, MFn::Type nodeType);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MAYA_UTILS_H
