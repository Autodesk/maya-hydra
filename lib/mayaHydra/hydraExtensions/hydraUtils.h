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

#ifndef MAYAHYDRALIB_HYDRA_UTILS_H
#define MAYAHYDRALIB_HYDRA_UTILS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/usd/sdf/path.h>

#include <string>

namespace MAYAHYDRA_NS_DEF {

/**
 * @brief Return the \p VtValue type and value as a string for debugging purposes.
 *
 * @param[in] val is the \p VtValue to be converted.
 * 
 * @return The \p VtValue type and value in string form.
 */
MAYAHYDRALIB_API
std::string ConvertVtValueToString(const PXR_NS::VtValue& val);

/**
 * @brief Strip \p nsDepth namespaces from \p nodeName.
 *
 * This will turn "taco:foo:bar" into "foo:bar" for \p nsDepth == 1, or "taco:foo:bar" into
 * "bar" for \p nsDepth > 1. If \p nsDepth is -1, all namespaces are stripped.
 *
 * @param[in] nodeName is the node name from which to strip namespaces.
 * @param[in] nsDepth is the namespace depth to strip.
 *
 * @return The stripped version of \p nodeName.
 */
MAYAHYDRALIB_API
std::string StripNamespaces(const std::string& nodeName, const int nsDepth = -1);

/**
 * @brief Replaces the invalid characters for SdfPath in-place in \p inOutPathString.
 *
 * @param[in,out] inOutPathString is the path string to sanitize.
 * @param[in] doStripNamespaces determines whether to strip namespaces or not.
 */
MAYAHYDRALIB_API
void SanitizeNameForSdfPath(std::string& inOutPathString, bool doStripNamespaces = false);

/**
 * @brief Replaces the invalid characters for SdfPath in \p pathString.
 *
 * @param[in] pathString is the path string to sanitize.
 * @param[in] doStripNamespaces determines whether to strip namespaces or not.
 *
 * @return The sanitized version of \p pathString.
 */
MAYAHYDRALIB_API
std::string SanitizeNameForSdfPath(const std::string& pathString, bool doStripNamespaces = false);

/**
 * @brief Get the given SdfPath without its parent path.
 *
 * The result is the last element of the original SdfPath.
 *
 * @param[in] path is the SdfPath from which to remove the parent path.
 *
 * @return The path without its parent path.
 */
MAYAHYDRALIB_API
PXR_NS::SdfPath MakeRelativeToParentPath(const PXR_NS::SdfPath& path);

/**
 * @brief Get the Hydra Xform matrix from a given prim.
 *
 * This method makes no guarantee on whether the matrix is flattened or not.
 *
 * @param[in] prim is the Hydra prim in the SceneIndex of which to get the transform matrix.
 * @param[out] outMatrix is the transform matrix of the prim.
 *
 * @return True if the operation succeeded, false otherwise.
 */
MAYAHYDRALIB_API
bool GetXformMatrixFromPrim(const PXR_NS::HdSceneIndexPrim& prim, PXR_NS::GfMatrix4d& outMatrix);

/**
 * @brief Get a directional light position from a direction vector.
 *
 * A directional light without a position does not seem to be supported by
 * Hydra at time of writing (6-May-2024).  Simulate a directional light by
 * positioning a light far away.
 *
 * @param[in] direction
 * @param[out] outPosition computed distant light position.
 */
MAYAHYDRALIB_API
void GetDirectionalLightPositionFromDirectionVector(PXR_NS::GfVec3f& outPosition, const PXR_NS::GfVec3f& direction);

//! Split the input source name <p srcName> into a base name <p base> and a
//! numerical suffix (if present) <p suffix>.
//! Returns true when numerical suffix was found, otherwise false.
MAYAHYDRALIB_API
bool splitNumericalSuffix(const std::string& srcName, std::string& base, std::string& suffix);

//! Return a name based on <p srcName> that is not in the set of <p
//! existingNames>.  If srcName is not in existingNames, it is returned
//! unchanged.  If it is in existingNames, try to extract a numerical suffix
//! from srcName (set to 1 if none).  The resulting name is checked against
//! existingNames, with the suffix incremented until the resulting name is
//! unique.
MAYAHYDRALIB_API
std::string uniqueName(const PXR_NS::TfToken::HashSet& existingNames, std::string srcName);

//! Use uniqueName() to return a name based on <p childName> that is not in the
//! existing children of <p parent>.
MAYAHYDRALIB_API
PXR_NS::TfToken uniqueChildName(
    const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex,
    const PXR_NS::SdfPath&                parent,
    const std::string&                    childName
);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_HYDRA_UTILS_H
