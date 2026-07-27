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
// Compares stored HdMeshTopology / HdBasisCurvesTopology against incoming MGeometry
// primitive data and implements RenderItemShouldEmitTopologyLocators for the
// render-item UpdateFromDelta path (see doc/render_delegate_topology_vs_deformation.md).
//
#include <mayaHydraLib/adapters/renderItemTopologyUtil.h>

#include <pxr/imaging/hd/basisCurvesTopology.h>
#include <pxr/imaging/hd/meshTopology.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

// Cost / necessity: this is a linear scan over the stored vs. new
// index/count arrays, so it is not free on a dense mesh. It is reached only through the narrow
// branch in RenderItemShouldEmitTopologyLocators below where Maya sets BOTH MVS_changedTopo and
// MVS_changedGeometry on the same update with an unchanged vertex count and a valid stored
// baseline (every other case short-circuits before this call). That combination is the
// MAYA-134200 corner case: MVS_changedTopo can be set on edits that do not actually change
// connectivity (e.g. a vertex move sets it alongside MVS_changedGeometry — see "Render item
// topology suppression" in doc/render_delegate_topology_vs_deformation.md), and Maya exposes no
// cheaper signal (no connectivity generation counter, no "did the index buffer change" bit) to
// disambiguate that from a real edit like an edge flip with the same vertex count. The stored
// _topology and the freshly-read vertexIndices/vertexCounts compared here are the only sources
// of truth available — there is no cheaper answer sitting elsewhere that this call would be
// redundantly recomputing. In practice this does not hit the animation/deformation path
// (skinning/blend shapes set geomChanged without topoChanged), only occasional topology-adjacent
// Maya operations (extrude, merge, edge flip, smooth-level crossing) with a stable vertex count.
bool RenderItemTopologyConnectivityChanged(
    const HdTopology*    storedTopology,
    MGeometry::Primitive primitive,
    const VtIntArray&    newIndices,
    const VtIntArray&    newCounts,
    size_t               lineStripVertexCount)
{
    // Without a stored baseline we cannot detect a connectivity delta on the geomChanged path.
    // Initial sync relies on positionsHaveBeenReset / topo-only flags in
    // RenderItemShouldEmitTopologyLocators.
    if (!storedTopology) {
        return false;
    }
    switch (primitive) {
    case MGeometry::Primitive::kTriangles:
    case MGeometry::Primitive::kTriangleStrip: {
        const auto* meshTopo = static_cast<const HdMeshTopology*>(storedTopology);
        return meshTopo->GetFaceVertexCounts() != newCounts
            || meshTopo->GetFaceVertexIndices() != newIndices;
    }
    case MGeometry::Primitive::kLines: {
        const auto* curveTopo = static_cast<const HdBasisCurvesTopology*>(storedTopology);
        return curveTopo->GetCurveVertexCounts() != newCounts
            || curveTopo->GetCurveIndices() != newIndices;
    }
    case MGeometry::Primitive::kLineStrip: {
        const auto* curveTopo = static_cast<const HdBasisCurvesTopology*>(storedTopology);
        VtIntArray expectedCounts;
        if (lineStripVertexCount > 0) {
            expectedCounts.assign(1, static_cast<int>(lineStripVertexCount));
        }
        return curveTopo->GetCurveVertexCounts() != expectedCounts;
    }
    default: return true;
    }
}

bool RenderItemShouldEmitTopologyLocators(
    bool                 topoChanged,
    bool                 geomChanged,
    bool                 hasGeomAndBuffers,
    bool                 positionsEmpty,
    size_t               storedPositionCount,
    unsigned int         currentVertexCount,
    const HdTopology*    storedTopology,
    MGeometry::Primitive primitive,
    const VtIntArray&    newIndices,
    const VtIntArray&    newCounts)
{
    if (!topoChanged && !geomChanged) {
        return false;
    }
    if (topoChanged && !geomChanged) {
        return true;
    }
    if (!hasGeomAndBuffers || positionsEmpty) {
        return topoChanged;
    }
    if (storedPositionCount != currentVertexCount) {
        return true;
    }
    // Line strips have no index buffer; connectivity is fully determined by vertex count.
    // storedPositionCount == currentVertexCount was already verified above, so real topology
    // changes (point count) were handled there. The MAYA-134200 "topo+geom flags set but
    // connectivity unchanged" case does not apply here the way it does for meshes (e.g. edge
    // flip with same vertex count). RenderItemTopologyConnectivityChanged's kLineStrip branch
    // would always return false on this path; it remains for direct callers / API completeness.
    if (primitive == MGeometry::Primitive::kLineStrip) {
        return false;
    }
    return RenderItemTopologyConnectivityChanged(
        storedTopology, primitive, newIndices, newCounts, storedPositionCount);
}

} // namespace MAYAHYDRA_NS_DEF
