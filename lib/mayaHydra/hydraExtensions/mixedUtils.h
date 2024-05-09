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

#ifndef MAYAHYDRALIB_MIXED_UTILS_H
#define MAYAHYDRALIB_MIXED_UTILS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/sceneIndex.h>

#include <maya/MFloatMatrix.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MMatrix.h>
#include <maya/MObject.h>

#if defined(_WIN32)
#include <windows.h>
#include "psapi.h"
#elif defined(__linux__)
#include <fstream>
#include <sstream>
#elif defined(__APPLE__)
#include <mach/task.h>
#include <mach/mach.h>
#endif

namespace MAYAHYDRA_NS_DEF {

constexpr int MB = 1024 * 1024;

// Function to get memory usage of the current process
MAYAHYDRALIB_API
inline int getProcessMemory()
{
#if defined(_WIN32)
    // see https://learn.microsoft.com/en-us/windows/win32/api/psapi/nf-psapi-getprocessmemoryinfo
    PROCESS_MEMORY_COUNTERS_EX pmc;
    GetProcessMemoryInfo(GetCurrentProcess(), (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc));
    return static_cast<int>(pmc.PrivateUsage / MB); // Working Set Size in MB

#elif defined(__linux__)
    // https://man7.org/linux/man-pages/man5/proc.5.html
    // When a process accesses this magic symbolic link, it
    // resolves to the process's own /proc/pid directory.
    long rss = 0L;
    FILE* fp = NULL;
    if ( (fp = fopen( "/proc/self/statm", "r" )) == NULL )
        return (size_t)0L;     
    if ( fscanf( fp, "%*s%ld", &rss ) != 1 )
    {
        fclose( fp );
        return (size_t)0L;
    }
    fclose( fp );
    return ((size_t)rss * (size_t)sysconf( _SC_PAGESIZE)) / MB ;
    
#elif defined(__APPLE__)
    // https://developer.apple.com/documentation/kernel/1537934-task_info
    task_vm_info_data_t vmInfo;
    mach_msg_type_number_t count = TASK_VM_INFO_COUNT;
    kern_return_t kernResult = task_info(mach_task_self(), TASK_VM_INFO, (task_info_t)&vmInfo, &count);

    if (kernResult == KERN_SUCCESS) {
        return static_cast<int>(vmInfo.virtual_size / MB); // Physical footprint in MB
    } else {
        return 0; // Unable to retrieve memory usage
    }
#endif
}

/**
 * @brief Converts a Maya matrix to a double precision GfMatrix.
 *
 * @param[in] mayaMat Maya `MMatrix` to be converted.
 *
 * @return `GfMatrix4d` equal to \p mayaMat.
 */
inline pxr::GfMatrix4d GetGfMatrixFromMaya(const MMatrix& mayaMat)
{
    pxr::GfMatrix4d mat;
    memcpy(mat.GetArray(), mayaMat[0], sizeof(double) * 16);
    return mat;
}

/**
 * @brief Converts a Maya float matrix to a double precision GfMatrix.
 *
 * @param[in] mayaMat Maya `MFloatMatrix` to be converted.
 *
 * @return `GfMatrix4d` equal to \p mayaMat.
 */
inline pxr::GfMatrix4d GetGfMatrixFromMaya(const MFloatMatrix& mayaMat)
{
    pxr::GfMatrix4d mat;
    for (unsigned i = 0; i < 4; ++i) {
        for (unsigned j = 0; j < 4; ++j)
            mat[i][j] = mayaMat(i, j);
    }
    return mat;
}

/**
 * @brief Returns the texture file path from a "file" shader node.
 *
 * @param[in] fileNode "file" shader node.
 *
 * @return Full path to the texture pointed used by the file node. `<UDIM>` tags are kept intact.
 */
MAYAHYDRALIB_API
pxr::TfToken GetFileTexturePath(const MFnDependencyNode& fileNode);

/**
 * @brief Determines whether or not a given DagPath refers to a shape.
 *
 * @param[in] dagPath is the DagPath to the potential shape.
 *
 * @return True if the dagPath refers to a shape, false otherwise.
 */
MAYAHYDRALIB_API
bool IsShape(const MDagPath& dagPath);

/**
 * @brief Converts the given Maya MDagPath \p dagPath into an SdfPath.
 *
 * Elements of the path will be sanitized such that it is a valid SdfPath. If \p mergeTransformAndShape 
 * is true and \p dagPath is a shape node, it will return the parent SdfPath of the shape's SdfPath, 
 * such that the transform and the shape have the same SdfPath.
 *
 * @param[in] dagPath is the DAG path to convert to an SdfPath.
 * @param[in] mergeTransformAndShape determines whether or not to consider the transform and shape
 * paths as one.
 * @param[in] stripNamespaces determines whether or not to strip namespaces from the path.
 *
 * @return The SdfPath corresponding to the given DAG path.
 */
MAYAHYDRALIB_API
pxr::SdfPath DagPathToSdfPath(
    const MDagPath& dagPath,
    const bool      mergeTransformAndShape,
    const bool      stripNamespaces);

/**
 * @brief Creates an SdfPath from the given Maya MRenderItem.
 *
 * Elements of the path will be sanitized such that it is a valid SdfPath.
 *
 * @param[in] ri is the MRenderItem to create the SdfPath for.
 * @param[in] stripNamespaces determines whether or not to strip namespaces from the path.
 *
 * @return The SdfPath corresponding to the given MRenderItem.
 */
MAYAHYDRALIB_API
pxr::SdfPath RenderItemToSdfPath(const MRenderItem& ri, const bool stripNamespaces);

/**
 * @brief Retrieves an RGB color preference from Maya.
 *
 * @param[in] colorName is the color name in Maya to get the color value of.
 * @param[out] outColor is the color that will be populated if retrieved from Maya.
 *
 * @return True if the color retrieval was successful and outColor was populated, false otherwise.
 */
MAYAHYDRALIB_API
bool getRGBAColorPreferenceValue(const std::string& colorName, PXR_NS::GfVec4f& outColor);

/**
 * @brief Retrieves an indexed color preference's index from Maya.
 *
 * @param[in] colorName is the color name in Maya to get the color index of.
 * @param[in] tableName is to indicate for which table/palette we want to get the index.
 * @param[out] outIndex is the index value that will be populated if retrieved from Maya.
 *
 * @return True if the color index retrieval was successful and outIndex was populated, false
 * otherwise.
 */
MAYAHYDRALIB_API
bool getIndexedColorPreferenceIndex(
    const std::string& colorName,
    const std::string& tableName,
    size_t&            outIndex);

/**
 * @brief Retrieves a palette color from Maya's color settings.
 *
 * @param[in] tableName is to indicate which table/palette we want to get the color from.
 * @param[in] index is to indicate which color to get from the palette.
 * @param[out] outColor is the color that will be populated if retrieved from Maya.
 *
 * @return True if the color retrieval was successful and outColor was populated, false otherwise.
 */
MAYAHYDRALIB_API
bool getColorPreferencesPaletteColor(
    const std::string& tableName,
    size_t             index,
    PXR_NS::GfVec4f&   outColor);

/**
 * @brief Retrieves an indexed/paletted color preference from Maya.
 *
 * @param[in] colorName is the color name in Maya to get the color value of.
 * @param[in] tableName is to indicate which table/palette we want to get the color from.
 * @param[out] outColor is the color that will be populated if retrieved from Maya.
 *
 * @return True if the color retrieval was successful and outColor was populated, false otherwise.
 */
MAYAHYDRALIB_API
bool getIndexedColorPreferenceValue(
    const std::string& colorName,
    const std::string& tableName,
    PXR_NS::GfVec4f&   outColor);

//! Using a standard suffix and the depend node type, call uniqueChildName() to
//! create a unique scene index path prefix based at the root of the scene
//! index scene.  The mayaNode MObject is passed by non-const reference to
//! satisfy MFnDependencyNode API requirements.
MAYAHYDRALIB_API
PXR_NS::SdfPath sceneIndexPathPrefix(
    const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex,
    MObject&                              mayaNode
);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_MIXED_UTILS_H
