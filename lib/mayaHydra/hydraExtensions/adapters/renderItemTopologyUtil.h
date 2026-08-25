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
// Topology dirty-policy helpers for the MayaHydra render-item adapter (MRenderItem path).
// RenderItemTopologyConnectivityChanged compares stored HdTopology against new MGeometry
// indices/counts; RenderItemShouldEmitTopologyLocators decides whether UpdateFromDelta
// should emit topology locators or treat the edit as deformation-only.
//
#ifndef MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H
#define MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H

#include <mayaHydraLib/api.h>

#include <pxr/imaging/hd/topology.h>
#include <pxr/base/vt/array.h>

#include <maya/MHWGeometry.h>

#include <cstddef>

namespace MAYAHYDRA_NS_DEF {

/// Returns true when \p newIndices / \p newCounts differ from \p storedTopology.
/// Only called from the narrow MAYA-134200 disambiguation branch in
/// RenderItemShouldEmitTopologyLocators (topo + geom changed together, vertex count stable);
/// see the cost/necessity comment above the definition in renderItemTopologyUtil.cpp.
MAYAHYDRALIB_API
bool RenderItemTopologyConnectivityChanged(
    const PXR_NS::HdTopology*   storedTopology,
    MGeometry::Primitive        primitive,
    const PXR_NS::VtIntArray&   newIndices,
    const PXR_NS::VtIntArray&   newCounts,
    size_t                      lineStripVertexCount);

/// Policy for whether UpdateFromDelta should emit mesh/topology locators on the render item path.
/// \p newIndices / \p newCounts are only populated when \p topoChanged is true, which is why the
/// geometry-only path returns early instead of comparing connectivity.
MAYAHYDRALIB_API
bool RenderItemShouldEmitTopologyLocators(
    bool                        topoChanged,
    bool                        geomChanged,
    bool                        hasGeomAndBuffers,
    bool                        positionsEmpty,
    size_t                      storedPositionCount,
    unsigned int                currentVertexCount,
    const PXR_NS::HdTopology*   storedTopology,
    MGeometry::Primitive        primitive,
    const PXR_NS::VtIntArray&   newIndices,
    const PXR_NS::VtIntArray&   newCounts);

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRALIB_RENDER_ITEM_TOPOLOGY_UTIL_H
