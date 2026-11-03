//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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
#ifndef MAYAHYDRA_RENDER_ITEM_PAYLOAD_H
#define MAYAHYDRA_RENDER_ITEM_PAYLOAD_H

#include <mayaHydraLib/api.h>
#if defined(USD_HAS_GPU_BUFFER_SHARING)
#include <mayaHydraLib/adapters/mhExtGpuStreams.h>
#endif

#include <maya/MDagPath.h>
#include <maya/MUserData.h>

#include <pxr/pxr.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>

// -----------------------------------------------------------------------------
// The Hydra payload riding a carrier render item as MUserData.
//
// Maya creates a carried render item for each drawn part of a claimed shape,
// and MayaHydraRenderItemOverride (renderItemOverrideCreator.cpp) attaches this
// object to it with MRenderItem::setCustomData(MSharedPtr<MUserData>). The
// consumer harvests it in UpdateRenderItems and drives a
// MayaHydraRenderItemOverrideAdapter from it.
//
// PLAIN ARRAYS, NOT DATA SOURCES (revised 2026-09-04)
// ---------------------------------------------------
// This used to carry pre-built HdContainerDataSources that the consumer pushed
// straight into the retained scene index. That bypassed the adapter registry and
// silently broke everything keyed on it - picking first and foremost (see
// MayaHydraRenderItemOverrideAdapter for the details). The payload is now the geometry
// itself, and MayaHydraRenderItemOverrideAdapter serves it through the machinery that
// already exists for Maya render items.
//
// TWO PAYLOADS PER SHAPE, one per carried render item: a shaded mesh and a
// wireframe, distinguished by kind. Maya carries two items so each can carry
// its own visibility - "wireframe on, shaded off" is a real state in a
// wireframe-only panel, and is exactly what VP2 expresses by giving
// DormantPolyWire and the shaded item different draw modes.
// -----------------------------------------------------------------------------
class MAYAHYDRALIB_API MayaHydraRenderItemPayload : public MUserData
{
public:
    MayaHydraRenderItemPayload() = default;
    ~MayaHydraRenderItemPayload() override = default;

    // Mirrors MRenderItemOverrideGeometry::Kind.
    enum Kind
    {
        kShadedMesh = 0,
        kWireframe  = 1,

        // A curve OBJECT - nurbsCurve, bezierCurve - not a mesh's wireframe.
        //
        // Carries the same arrays as kWireframe (points + edgeIndices), and is
        // drawn the same way: unlit lines in the wireframe colour, which is
        // what VP2 does for a curve too. What differs is MEANING. A wireframe
        // is decoration over a surface that exists in the same scene; a curve
        // is the object, it draws in a shaded viewport, and it owns its DAG
        // identity for selection.
        //
        // Kept distinct rather than folded into kWireframe so the two can
        // diverge - picking and material resolution are the obvious places -
        // without having to work out afterwards which line lists were which.
        kCurve      = 2,

        // The object drawn as its bounding box (bounding-box display mode, or a
        // bounding-box level of detail): eight corners and twelve edges, the
        // carrier counterpart of VP2's BoundingBoxItem. Drawn like a wireframe;
        // while it is drawn the producer hides the object's other carriers.
        kBoundingBox = 3
    };

    //! The kinds whose geometry is a line list rather than a surface.
    bool isLineList() const
    {
        return kind == kWireframe || kind == kCurve || kind == kBoundingBox;
    }

    Kind kind = kShadedMesh;

    // The Maya shape this came from. The consumer needs it for everything the
    // producer cannot resolve on its own - the transform, the material, the
    // wireframe colour, and the DAG path picking resolves back to. A carried
    // render item has no MRenderItem::sourceDagPath(), so this is the only
    // source. Kept up to date by updateRenderItem across renames/reparents.
    MDagPath dagPath;

    // Geometry, in VP2's "render vertex" layout: points and normals are
    // both VERTEX interpolated over the same array and the index arrays index
    // into it, so it maps onto Hydra with no re-indexing.
    //
    // points is SHARED between the two payloads of a shape - VtArray is
    // copy-on-write refcounted and the override builds it once per update - so the
    // wireframe costs a refcount, not a second copy of every vertex.
    PXR_NS::VtVec3fArray points;
    PXR_NS::VtVec3fArray normals;   // may be empty
    PXR_NS::VtVec2fArray uvs;       // may be empty

    // kShadedMesh only.
    PXR_NS::VtIntArray faceVertexCounts;
    PXR_NS::VtIntArray faceVertexIndices;

    // kShadedMesh only: Maya polygon p became faces
    // [polygonFaceOffsets[p], polygonFaceOffsets[p + 1]) of the topology above,
    // which is VP2's triangulation. What maps a shading group's polygon
    // membership onto the published faces for its geomSubset. EMPTY when there
    // is no mapping (smooth mesh preview): the prim then keeps one material.
    PXR_NS::VtIntArray polygonFaceOffsets;

    // kShadedMesh only: CACHED-PLAYBACK MOTION BLUR. The points at the shutter
    // open / close Maya built from cached frames, at the offsets (frames) it
    // built them for. Empty when Maya had none for this publish - the adapter
    // then falls back to velocity extrapolation.
    PXR_NS::VtVec3fArray shutterOpenPoints;
    PXR_NS::VtVec3fArray shutterClosePoints;
    float        shutterOpenOffset  = 0.0f;
    float        shutterCloseOffset = 0.0f;

    // kWireframe only: vertex pairs indexing the same dedup points, produced by
    // VP2's own edge list so hard/soft and shared-edge behaviour
    // matches what VP2 draws.
    PXR_NS::VtIntArray edgeIndices;

    // Object-space extent, computed by Maya (MRenderItemOverrideGeometry::bounds,
    // which says why the consumer does not derive it from `points`). Stale while
    // vertexBufferOnGPU is set: it is the last CPU-side geometry's.
    PXR_NS::GfRange3d bounds;

    // GPU-DEFORMED OBJECTS, when the override asked for device data
    // (MPxRenderItemOverride::supportsGPUVertexBuffer). `points` and `normals`
    // are then EMPTY and the buffers the deformer wrote are described below
    // instead; uvs and topology are as usual. Can flip either way between two
    // geometry changes of the same object.
    //
    // Stored as plain descriptions rather than MVertexBuffer pointers: the handle
    // Maya hands out is borrowed for the duration of updateRenderItem, and the
    // adapter only runs later, in UpdateRenderItems. Maya reports any change of
    // buffer with the next geometry change, which refreshes these.
    bool vertexBufferOnGPU = false;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
    PXR_NS::MhExtGpuSource devicePoints;
    PXR_NS::MhExtGpuSource deviceNormals;   // shaded mesh only
    // A device copy of `uvs`, which stays filled too. Shared so that every vertex
    // stream of the prim is an external buffer: Storm will not bind some of a
    // prim's vertex streams in place while copying others, so a CPU st next to
    // shared points sends them all down its copy path. Shaded mesh with UVs only.
    PXR_NS::MhExtGpuSource deviceUVs;
#endif
};

#endif // MAYAHYDRA_RENDER_ITEM_PAYLOAD_H
