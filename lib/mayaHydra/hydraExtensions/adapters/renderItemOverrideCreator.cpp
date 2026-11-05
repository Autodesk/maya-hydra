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
#include "renderItemOverrideCreator.h"

#include "renderItemPayload.h"

#include <mayaHydraLib/adapters/renderItemAdapter.h>  // UseVelocityMotionSamples
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <maya/MSharedPtr.h>
#include <maya/MStatus.h>
#include <maya/MString.h>
#include <maya/MUserData.h>
#include <maya/MViewport2Renderer.h>

#include <iterator>
#include <vector>

#if defined(MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE)
// Maya's PRIVATE render-item override API: unsupported, undocumented, and free
// to change - including breaking changes - within a Maya major release. Each
// MayaHydra branch tracks one Maya release line and is rebuilt for every Maya
// update, so a change shipped in an update release is handled with a
// MAYA_API_VERSION check. On Maya's development branch MAYA_API_VERSION does
// not move, so this file changes in lockstep with the header there.
#include <maya/MPxRenderItemOverridePrivate.h>
#endif

#include <pxr/pxr.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec2f.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>

PXR_NAMESPACE_USING_DIRECTIVE

// =============================================================================
// Render item override consumer, on Maya's PRIVATE MPxRenderItemOverride API
// (maya/MPxRenderItemOverridePrivate.h). Compiled only when FindMaya finds that
// header (MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE); otherwise it is empty, the
// plugin registers nothing and the stock render-item path runs.
//
// Downstream, the payload is a MayaHydraRenderItemPayload harvested in
// MayaHydraSceneIndex::UpdateRenderItems and served through
// MayaHydraRenderItemOverrideAdapter, so picking, materials, bounding boxes
// and the two-items-per-shape wireframe design are the render item adapter's.
// =============================================================================

#if defined(MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE)

namespace {

using MVS = MHWRender::MDataServerOperation::MViewportScene;
using MHWRender::MRenderItemOverrideBuffer;
using MHWRender::MRenderItemOverrideIndexBuffer;
using MHWRender::MRenderItemOverrideGeometry;

// Every draw classification the override is registered for. Maya resolves
// overrides by classification prefix, so each entry covers every shape of
// that type.
//
// One entry per Maya shape type Maya can carry. Registering for a type Maya
// cannot carry is harmless - the object falls through to the ordinary VP2
// path - but it is also pointless.
const MString kDrawClassifications[] = {
    MString("drawdb/geometry/mesh"),
    MString("drawdb/geometry/nurbsCurve"),
    MString("drawdb/geometry/bezierCurve"),
    MString("drawdb/geometry/nurbsSurface"),
    MString("drawdb/geometry/subdiv"),
};

// ---------------------------------------------------------------------------
// Our payload type. Kept distinct from the public MayaHydraRenderItemPayload base so
// updateRenderItem can dynamic_cast to exactly what this override attached. The
// scene index harvests through the base.
// ---------------------------------------------------------------------------
class MhBridgeRenderItemPayload : public MayaHydraRenderItemPayload
{
public:
    // Nothing extra: the public base already carries everything the adapter
    // reads. The subclass exists purely as a type tag.
};

// A VtArray over a vertex array Maya lends (MRenderItemOverrideBuffer) instead of
// a copy of it - zero-copy hand-off of positions and normals.
//
// VtArray's foreign data source: the arrays sharing it reference Maya's memory,
// and when the last one goes VtArray calls back, which drops our hold on the
// buffer. Maya never writes a buffer anyone still holds - it fills the next
// frame into another - so the data stays exactly what it was for as long as any
// array (ours, Hydra's, a renderer's) still points at it. Anything that wants to
// modify such an array gets a copy (VtArray copy-on-write), as usual.
//
// Handle is MRenderItemOverrideBuffer (vertex fields) or
// MRenderItemOverrideIndexBuffer (topology arrays).
template <class Handle>
class LentBuffer : public Vt_ArrayForeignDataSource
{
public:
    template <class T>
    static VtArray<T> Wrap(const Handle& lent, size_t n)
    {
        auto* source = new LentBuffer(lent);
        auto* data   = const_cast<T*>(reinterpret_cast<const T*>(lent.data()));
        return VtArray<T>(source, data, n);
    }

private:
    explicit LentBuffer(const Handle& lent)
        : Vt_ArrayForeignDataSource(&_Detached)
        , _lent(lent)
    {
    }

    static void _Detached(Vt_ArrayForeignDataSource* self)
    {
        delete static_cast<LentBuffer*>(self);
    }

    Handle _lent;
};
using LentVertexBuffer = LentBuffer<MRenderItemOverrideBuffer>;

// A topology array (face counts, face indices, edges): lent when Maya can,
// copied otherwise. Re-sent only on a topology change, and then shared with
// Maya - and, through the VtArray, between this object's payloads - instead of
// duplicated.
VtIntArray lendOrCopyIndices(const int* src, size_t count, const MRenderItemOverrideIndexBuffer& lent)
{
    if (count == 0 || !src) {
        return VtIntArray();
    }
    if (!lent.isNull() && lent.data() == src && lent.size() >= count) {
        return LentBuffer<MRenderItemOverrideIndexBuffer>::Wrap<int>(lent, count);
    }
    return VtIntArray(src, src + count);
}

// Copy a vertex field (positions or normals) once per pull, and SHARE the result
// between every payload of the shape that the pull updates: the shaded and
// wireframe items, and the item pairs of every further instance of an instanced
// shape (per-instance carriers). VtArray is copy-on-write refcounted, so handing
// back the same array costs a refcount instead of a copy per item.
//
// Range-construct rather than size-construct + memcpy: the latter
// value-initialises the whole array (a full 3*N float zero-fill) only to
// overwrite it immediately. At ~632k vertices per frame across a rig that is
// several MB of pointless writes per mesh per frame.
//
// THE MEMO IS SCOPED TO ONE PULL, AND MUST BE. Keying purely on (source pointer,
// vertex count) is not a content key: the pointer is the address of a buffer
// Maya reuses, stable for the object's lifetime, while the CONTENTS are refilled
// every frame - so a hit across two frames handed back the PREVIOUS frame's
// vertices (a GPU-deformed mesh, refreshed alone every frame, sat in its rest
// pose). Maya stamps every update with a per-object pull id
// (MRenderItemOverrideGeometry::pullId) and draws a new one for each pull, including
// a deformation republish, so (pull id, source pointer, count) does identify the
// data. Reuse only on that full match; pull id 0 means unknown, never reuse.
//
// Any item kind may reuse: within one pull Maya hands every item of the shape
// the same arrays. The source pointer still distinguishes the arrays that are
// NOT shared - a NURBS surface's wireframe points at its isoparm vertices, not
// at the tessellation the shaded item carries.
class VertexFieldMemo
{
public:
    // `lent` is Maya's shared ownership of `src` (MRenderItemOverrideGeometry::
    // positionsBuffer / normalsBuffer): when it is there the array references
    // Maya's memory instead of copying it.
    VtVec3fArray get(
        const float*                     src,
        unsigned int                     n,
        unsigned int                     pullId,
        const MRenderItemOverrideBuffer& lent)
    {
        if (n == 0 || !src) {
            return VtVec3fArray();
        }
        if (pullId != 0 && pullId == _pullId && src == _src && n == _count) {
            return _array;
        }

        if (!lent.isNull() && lent.data() == src && lent.size() >= 3u * static_cast<size_t>(n)) {
            _array = LentVertexBuffer::Wrap<GfVec3f>(lent, n);
        } else {
            const GfVec3f* begin = reinterpret_cast<const GfVec3f*>(src);
            _array = VtVec3fArray(begin, begin + n);
        }
        _src    = src;
        _count  = n;
        _pullId = pullId;
        return _array;
    }

private:
    const float* _src    = nullptr;
    unsigned int _count  = 0;
    unsigned int _pullId = 0;
    VtVec3fArray _array;
};

// One memo per field: positions and normals are separate arrays.
struct VertexFieldMemos
{
    VertexFieldMemo positions;
    VertexFieldMemo normals;
};

// No global instance: each MayaHydraRenderItemOverride (one per carried object)
// owns its memo, because updateRenderItem runs concurrently for DIFFERENT objects
// (supportsEvaluationManagerParallelUpdate) and the memo only ever reuses within one object.

#if defined(USD_HAS_GPU_BUFFER_SHARING)
// Describe one device-resident field. The handle Maya hands out is borrowed for
// this call only, so it is read out here, into a plain value the adapter can
// still use later in UpdateRenderItems.
MhExtGpuSource deviceSource(
    const MRenderItemOverrideGeometry& geometry,
    MHWRender::MGeometry::Semantic     semantic,
    const char*                        debugName)
{
    MHWRender::MVertexBufferDescriptor descriptor;
    void*                              resourceHandle = nullptr;
    if (!geometry.gpuVertexBuffer(semantic, descriptor, resourceHandle)) {
        return {};
    }
    return MhExtGpuSource::FromDescriptor(
        descriptor, resourceHandle, geometry.vertexCount(), debugName);
}
#endif

// Whether the item carries geometry this override can publish: CPU positions, or
// device-resident ones described instead. Empty means "not ready".
bool hasGeometry(const MRenderItemOverrideGeometry& geometry)
{
    return geometry.vertexCount() > 0 && (geometry.positions() || geometry.vertexBufferOnGPU());
}

// The shape lost its geometry (Maya publishes an empty view for that). Drop
// every array and device stream, so the prim stops drawing what it had.
template <class Data>
void clearRenderItemOverrideGeometry(Data& customData)
{
    customData.points             = VtVec3fArray();
    customData.normals            = VtVec3fArray();
    customData.uvs                = VtVec2fArray();
    customData.faceVertexCounts   = VtIntArray();
    customData.faceVertexIndices  = VtIntArray();
    customData.polygonFaceOffsets = VtIntArray();
    customData.edgeIndices        = VtIntArray();
    customData.shutterOpenPoints  = VtVec3fArray();
    customData.shutterClosePoints = VtVec3fArray();
    customData.shutterOpenOffset  = 0.0f;
    customData.shutterCloseOffset = 0.0f;
    customData.bounds             = GfRange3d();
    customData.vertexBufferOnGPU = false;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
    customData.devicePoints  = MhExtGpuSource();
    customData.deviceNormals = MhExtGpuSource();
    customData.deviceUVs     = MhExtGpuSource();
#endif
}

// Fill a payload from the translated geometry. Only the arrays that kind actually uses
// are copied - the wireframe wants points and edges, not normals or UVs.
//
// withTopology gates the arrays that only move on a TOPOLOGY edit: face counts,
// face indices and the edge list. Copying those on every deformation frame is
// hundreds of KB per mesh per frame of pure waste. Points and normals follow
// deformation, so they are not gated. UVs are re-copied only on a topology
// change or when Maya reports them changed (uvsChanged); see below.
void fillRenderItemOverrideGeometry(
    MhBridgeRenderItemPayload&         customData,
    const MRenderItemOverrideGeometry& geometry,
    bool                               withTopology,
    VertexFieldMemos&                  memos)
{
    const unsigned int n = geometry.vertexCount();

    // A GPU-deformed object we asked device data for: Maya did not read the
    // deformed positions and normals back, so there is nothing to copy - only
    // the buffers to describe. Bounds are still taken below.
    customData.vertexBufferOnGPU = geometry.vertexBufferOnGPU();
    if (customData.vertexBufferOnGPU) {
        customData.points = VtVec3fArray();
#if defined(USD_HAS_GPU_BUFFER_SHARING)
        customData.devicePoints = deviceSource(geometry, MHWRender::MGeometry::kPosition, "position");
        customData.deviceNormals = customData.isLineList()
            ? MhExtGpuSource()
            : deviceSource(geometry, MHWRender::MGeometry::kNormal, "normal");
        customData.deviceUVs = customData.isLineList()
            ? MhExtGpuSource()
            : deviceSource(geometry, MHWRender::MGeometry::kTexture, "texture");
#endif
    } else {
#if defined(USD_HAS_GPU_BUFFER_SHARING)
        customData.devicePoints = MhExtGpuSource();
        customData.deviceNormals = MhExtGpuSource();
        customData.deviceUVs = MhExtGpuSource();
#endif
        customData.points = memos.positions.get(
            geometry.positions(), n, geometry.pullId(), geometry.positionsBuffer());
    }

    // Bounds for BOTH kinds. A device payload carries the GPU deformer's own
    // box, or an EMPTY one (min > max) when Maya has none, in which case the
    // adapter falls back to the DAG node (see MRenderItemOverrideGeometry::bounds).
    {
        float mn[3] = { 0.0f, 0.0f, 0.0f };
        float mx[3] = { 0.0f, 0.0f, 0.0f };
        geometry.bounds(mn, mx);
        customData.bounds
            = GfRange3d(GfVec3d(mn[0], mn[1], mn[2]), GfVec3d(mx[0], mx[1], mx[2]));
    }

    if (customData.isLineList()) {
        if (withTopology) {
            const unsigned int count = geometry.edgeIndexCount();
            const int*         src   = geometry.edgeIndices();
            customData.edgeIndices = (count >= 2 && src)
                ? lendOrCopyIndices(src, count, geometry.edgeIndicesBuffer())
                : VtIntArray();
        }
        return;
    }

    // Cached-playback shutter points, when Maya built them for this publish.
    {
        const float* open  = geometry.shutterOpenPositions();
        const float* close = geometry.shutterClosePositions();
        if (open && close && n > 0 && !customData.vertexBufferOnGPU) {
            const GfVec3f* o = reinterpret_cast<const GfVec3f*>(open);
            const GfVec3f* c = reinterpret_cast<const GfVec3f*>(close);
            customData.shutterOpenPoints  = VtVec3fArray(o, o + n);
            customData.shutterClosePoints = VtVec3fArray(c, c + n);
            customData.shutterOpenOffset  = geometry.shutterOpenOffset();
            customData.shutterCloseOffset = geometry.shutterCloseOffset();
        } else {
            customData.shutterOpenPoints  = VtVec3fArray();
            customData.shutterClosePoints = VtVec3fArray();
            customData.shutterOpenOffset  = 0.0f;
            customData.shutterCloseOffset = 0.0f;
        }
    }

    // Null when the normals are device-resident, which leaves them empty here.
    if (const float* src = geometry.normals()) {
        // VERTEX, not faceVarying: Maya already split the vertices wherever a
        // normal is discontinuous, so hard edges are carried by the layout.
        customData.normals = memos.normals.get(src, n, geometry.pullId(), geometry.normalsBuffer());
    } else {
        customData.normals = VtVec3fArray();
    }

    // UVs only when they can have moved. Deformation never touches them, and
    // re-allocating + copying them for every deforming mesh on every frame was
    // a large share of the per-frame cost. customData persists across
    // updates of the same item, so the kept array stays current. A topology
    // change always re-copies (the vertex count may have moved).
    if (withTopology || geometry.uvsChanged()) {
        if (const float* src = geometry.uvs()) {
            // Lent when Maya can (MRenderItemOverrideGeometry::uvsBuffer): the array then
            // shares Maya's buffer, which Maya re-writes only when the UVs change -
            // one copy of the UVs instead of two for as long as they stay put.
            const MRenderItemOverrideBuffer lent = geometry.uvsBuffer();
            if (!lent.isNull() && lent.data() == src && lent.size() >= 2u * static_cast<size_t>(n)) {
                customData.uvs = LentVertexBuffer::Wrap<GfVec2f>(lent, n);
            } else {
                const GfVec2f* begin = reinterpret_cast<const GfVec2f*>(src);
                customData.uvs = VtVec2fArray(begin, begin + n);
            }
        } else {
            customData.uvs = VtVec2fArray();
        }
    }

    if (withTopology) {
        const unsigned int faces = geometry.faceCount();
        if (faces > 0 && geometry.faceVertexCounts() && geometry.faceVertexIndices()) {
            customData.faceVertexCounts = lendOrCopyIndices(
                geometry.faceVertexCounts(), faces, geometry.faceVertexCountsBuffer());
            customData.faceVertexIndices = lendOrCopyIndices(
                geometry.faceVertexIndices(), geometry.faceVertexIndexCount(),
                geometry.faceVertexIndicesBuffer());

            // With the topology it indexes, so the two can never disagree.
            const unsigned int  polys   = geometry.polygonCount();
            const unsigned int* offsets = geometry.polygonFaceOffsets();
            customData.polygonFaceOffsets = VtIntArray();
            if (polys > 0 && offsets) {
                customData.polygonFaceOffsets.resize(static_cast<size_t>(polys) + 1u);
                for (unsigned int p = 0; p <= polys; ++p) {
                    customData.polygonFaceOffsets[p] = static_cast<int>(offsets[p]);
                }
            }
        } else {
            // A topology change with no faces (the mesh was emptied, or the
            // arrays are missing): CLEAR the topology. Keeping the old arrays
            // paired them with this publish's points - indices past the new
            // vertex count, which Storm drops or draws as garbage.
            customData.faceVertexCounts   = VtIntArray();
            customData.faceVertexIndices  = VtIntArray();
            customData.polygonFaceOffsets = VtIntArray();
        }
    }
}

// ---------------------------------------------------------------------------
// The override. One instance per carried object.
// ---------------------------------------------------------------------------
class MayaHydraRenderItemOverride : public MHWRender::MPxRenderItemOverride
{
public:
    static MHWRender::MPxRenderItemOverride* creator(const MObject& node)
    {
        // Claim every object Maya routes here. A per-object filter would go HERE -
        // returning nullptr leaves an object to ordinary VP2 drawing - but
        // the render item override path is a whole-viewport mode, so declining an object would just make
        // it vanish from the Hydra viewport.
        return new MayaHydraRenderItemOverride(node);
    }

    void updateRenderItem(
        const MDagPath&                    path,
        MHWRender::MRenderItem&            item,
        const MRenderItemOverrideGeometry& geometry,
        unsigned int                       changeFlags) override
    {
        if (changeFlags & MVS::MVS_new) {
            createPayload(path, item, geometry);
            return;
        }

        const MSharedPtr<MUserData> userData = item.getCustomData();
        auto* customData = dynamic_cast<MhBridgeRenderItemPayload*>(userData.get());
        if (!customData) {
            return;
        }

        // Reparenting changes the path but not the object, and the adapter reads
        // this for the transform, the material and the DAG path picking resolves
        // back to. Cheap to refresh, since Maya hands it to us.
        //
        // Compared before assigning: the path almost never changes, and copying
        // an MDagPath writes node reference counts - from every worker at once
        // when updates run concurrently (supportsEvaluationManagerParallelUpdate).
        if (!(customData->dagPath == path)) {
            customData->dagPath = path;
        }

        // Refresh geometry ONLY when the delta actually touched it. A
        // presentation-only update (selection, display style) arrives with
        // NO geometry and must not clobber the arrays - which is exactly why this
        // is gated on the geometry bits rather than run unconditionally.
        //
        // A topology change implies the points too, since the vertex count moved.
        // It also covers a smooth-mesh-preview toggle: Maya folds that into a
        // geometry change and hands us the other mesh entirely.
        if (changeFlags & (MVS::MVS_changedGeometry | MVS::MVS_changedTopo)) {
            if (hasGeometry(geometry)) {
                fillRenderItemOverrideGeometry(
                    *customData, geometry,
                    /*withTopology*/ (changeFlags & MVS::MVS_changedTopo) != 0,
                    _memos);
            } else if (geometry.vertexCount() == 0) {
                // Emptied, not unready: an update only reaches here for an
                // item that was created with geometry.
                clearRenderItemOverrideGeometry(*customData);
            }
        }

        // Everything else - transform, visibility, wireframe colour and the dirty
        // notifications for all of it - is applied by the scene index and
        // MayaHydraRenderItemOverrideAdapter when the scene index harvests this render item. Those need the scene
        // index and the adapter, neither of which an override can see.
    }

private:
    // The first call for an item: build its payload and attach it to the item.
    // Attaching nothing declines the item, and Maya asks again later.
    void createPayload(
        const MDagPath&                    path,
        MHWRender::MRenderItem&            item,
        const MRenderItemOverrideGeometry& geometry)
    {
        // Geometry must come from Maya's own translation; there is no MFnMesh
        // fallback by design (it would silently regress to the faceted base
        // cage). Empty geometry means "not ready" - decline and be retried.
        if (!hasGeometry(geometry)) {
            return;
        }

        const MRenderItemOverrideGeometry::Kind gk = geometry.kind();
        const bool isLineList = (gk == MRenderItemOverrideGeometry::kWireframe)
                             || (gk == MRenderItemOverrideGeometry::kCurve)
                             || (gk == MRenderItemOverrideGeometry::kBoundingBox);

        // A line item is only worth a payload if there are edges to draw. For a
        // wireframe that means "nothing to overlay"; for a curve it means the
        // extractor produced no curve at all, which is equally not worth a prim.
        if (isLineList && (geometry.edgeIndexCount() < 2 || !geometry.edgeIndices())) {
            return;
        }

        auto customData = MSharedPtr<MhBridgeRenderItemPayload>::make();
        customData->dagPath = path;
        switch (gk) {
        case MRenderItemOverrideGeometry::kWireframe:
            customData->kind = MayaHydraRenderItemPayload::kWireframe;
            break;
        case MRenderItemOverrideGeometry::kCurve:
            customData->kind = MayaHydraRenderItemPayload::kCurve;
            break;
        case MRenderItemOverrideGeometry::kBoundingBox:
            customData->kind = MayaHydraRenderItemPayload::kBoundingBox;
            break;
        default: customData->kind = MayaHydraRenderItemPayload::kShadedMesh; break;
        }
        fillRenderItemOverrideGeometry(*customData, geometry, /*withTopology*/ true, _memos);
        item.setCustomData(customData);
    }

public:
    // GPU-deformed objects: take the deformer's buffers as they are instead of a
    // readback, which is a synchronous device map per field per evaluation. The
    // adapter shares them with the renderer, and serves CPU consumers through
    // its own lazy readback.
    //
    // Needs GPU buffer sharing (the adapter's only route to device data) and a
    // VP2 on OpenGL (the only API that readback and the bridges speak). The
    // draw API is fixed for the session, so it is asked once.
    bool supportsGPUVertexBuffer() const override
    {
#if defined(USD_HAS_GPU_BUFFER_SHARING)
        static const bool usable = [] {
            const MHWRender::MRenderer* renderer = MHWRender::MRenderer::theRenderer();
            return MhExtGpuStreams::IsEnabled() && renderer && renderer->drawAPIIsOpenGL();
        }();
        // And only for a renderer that binds device streams: one that cannot
        // would read them back anyway, and on CPU points the carrier's
        // readback, motion blur samples and the rest all apply. Maya asks on
        // every publish, so a renderer switch takes effect on the next one.
        return usable && MhExtGpuStreams::RendererBindsDeviceStreams();
#else
        return false;
#endif
    }

    // updateRenderItem touches only this item's payload (customData) and this
    // instance's memo - VtArray allocation is thread-safe - so Maya may run it
    // on a worker, concurrently with other objects. That moves the per-frame
    // point/normal conversion of every CPU-deformed mesh off the main thread.
    bool supportsEvaluationManagerParallelUpdate() const override { return true; }

private:
    explicit MayaHydraRenderItemOverride(const MObject& node)
        : MHWRender::MPxRenderItemOverride(node)
    {
    }

    // Per-pull sharing of positions and normals between THIS object's payloads;
    // see VertexFieldMemo.
    VertexFieldMemos _memos;
};

} // anonymous namespace

namespace MayaHydraRenderItemOverrideCreator {

MHWRender::MPxRenderItemOverride* create(const MObject& node)
{
    return MayaHydraRenderItemOverride::creator(node);
}

const std::vector<MString>& drawClassifications()
{
    static const std::vector<MString> classifications(
        std::begin(kDrawClassifications), std::end(kDrawClassifications));
    return classifications;
}

MHWRender::MPxRenderItemOverride::ShutterSamples shutterSamples()
{
    // The adapter prefers Maya's shutter points whenever they match the shutter
    // Hydra renders with: exact, and valid on any frame - scrubbed, paused or
    // the first - which its velocity fallback is not. Cached ones in either
    // sampling mode; evaluated ones too in evaluate mode, as the adapter
    // evaluates transforms at the shutter times there.
    return MayaHydraRenderItemAdapter::UseVelocityMotionSamples()
        ? MHWRender::MPxRenderItemOverride::kCachedShutterSamples
        : MHWRender::MPxRenderItemOverride::kEvaluatedShutterSamples;
}

} // namespace MayaHydraRenderItemOverrideCreator

#endif // MAYA_HAS_RENDER_ITEM_OVERRIDE_PRIVATE
