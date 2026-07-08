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

PXR_NAMESPACE_OPEN_SCOPE

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
    if (primitive == MGeometry::Primitive::kLineStrip) {
        return false;
    }
    return RenderItemTopologyConnectivityChanged(
        storedTopology, primitive, newIndices, newCounts, storedPositionCount);
}

PXR_NAMESPACE_CLOSE_SCOPE
