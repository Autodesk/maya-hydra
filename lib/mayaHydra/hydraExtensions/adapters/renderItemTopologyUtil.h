//
// Copyright 2026 Autodesk
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
#ifndef MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H
#define MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H

#include <mayaHydraLib/api.h>

#include <pxr/imaging/hd/topology.h>
#include <pxr/base/vt/array.h>

#include <maya/MHWGeometry.h>

#include <cstddef>

PXR_NAMESPACE_OPEN_SCOPE

/// Returns true when \p newIndices / \p newCounts differ from \p storedTopology.
MAYAHYDRALIB_API
bool RenderItemTopologyConnectivityChanged(
    const HdTopology*           storedTopology,
    MGeometry::Primitive        primitive,
    const VtIntArray&           newIndices,
    const VtIntArray&           newCounts,
    size_t                      lineStripVertexCount);

/// Policy for whether UpdateFromDelta should emit mesh/topology locators on the render item path.
MAYAHYDRALIB_API
bool RenderItemShouldEmitTopologyLocators(
    bool                        topoChanged,
    bool                        geomChanged,
    bool                        hasGeomAndBuffers,
    bool                        positionsEmpty,
    size_t                      storedPositionCount,
    unsigned int                currentVertexCount,
    const HdTopology*           storedTopology,
    MGeometry::Primitive        primitive,
    const VtIntArray&           newIndices,
    const VtIntArray&           newCounts);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H
