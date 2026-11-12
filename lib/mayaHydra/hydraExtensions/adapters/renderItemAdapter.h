//
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H
#define MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/tf/token.h>
#include <vector>

#include <pxr/imaging/hd/basisCurvesTopology.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/pxr.h>

#include <maya/MDagPath.h>
#include <maya/MHWGeometry.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MMatrix.h>

#include <functional>
#include <memory>
#include <optional>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

namespace {
std::string kRenderItemTypeName = "renderItem";

static constexpr const char* kPointSize = "pointSize";

static const SdfPath kInvalidMaterial = SdfPath("InvalidMaterial");

#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
// Extract doubleSided attribute from CullMode as MRenderItem uses CullNone to denote doubleSided.
static bool IsDoubleSided(MRenderItem::CullMode cullMode) { return cullMode == MRenderItem::CullNone; }
#endif
} // namespace

using MayaHydraRenderItemAdapterPtr = std::shared_ptr<class MayaHydraRenderItemAdapter>;

/**
 * \brief MayaHydraRenderItemAdapter is used to translate from a render item to hydra.
 * This is where we translate from Maya shapes (such as meshes) to hydra.
 */
class MayaHydraRenderItemAdapter : public MayaHydraAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraRenderItemAdapter(
        const MDagPath&       dagPath,
        const SdfPath&        slowId,
        int                   fastId,
        MayaHydraSceneIndex*  mayaHydraSceneIndex,
        const MRenderItem&    ri,
        TfToken               purposeRenderTag);

    MAYAHYDRALIB_API
    virtual ~MayaHydraRenderItemAdapter();

    MAYAHYDRALIB_API
    virtual void RemovePrim() override { }

    MAYAHYDRALIB_API
    virtual void Populate() override;

    MAYAHYDRALIB_API
    bool HasType(const TfToken& typeId) const override { return typeId == HdPrimTypeTokens->mesh; }

    MAYAHYDRALIB_API
    virtual bool IsSupported() const override;

    MAYAHYDRALIB_API
    bool GetDoubleSided() const override { 
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
        return IsDoubleSided(_cullMode);
#else
        return false;
#endif
    };

    MAYAHYDRALIB_API
    GfBBox3d GetBoundingBox() override { return _bounds; }

    MAYAHYDRALIB_API
    GfVec4f GetDisplayColor() const override { return {_wireframeColor.r, _wireframeColor.g, _wireframeColor.b, _wireframeColor.a}; }

    MAYAHYDRALIB_API
    HdCullStyle GetCullStyle() const override;

    MAYAHYDRALIB_API
    VtValue Get(const TfToken& key) override;

    /// Motion samples of a primvar across the shutter, the shape adapters'
    /// SamplePrimvar contract: fills up to \p maxSampleCount (time, value)
    /// pairs and returns how many. 0 means no samples - the live value stands.
    /// An ordinary render item has no way to re-evaluate its geometry at another
    /// time; a render item override carrier can (MayaHydraRenderItemOverrideAdapter).
    MAYAHYDRALIB_API
    virtual size_t SamplePrimvar(const TfToken&, size_t, float*, VtValue*) { return 0; }

    MAYAHYDRALIB_API
    VtValue GetMaterialResource();

    MAYAHYDRALIB_API
    void SetPlaybackState(bool isPlaybackRunning);

    MAYAHYDRALIB_API
    void SetWireframeSelectionHighlightEnabled(bool enabled);

    MAYAHYDRALIB_API
    bool GetVisible() override;

    MAYAHYDRALIB_API
    const MColor& GetWireframeColor() const { return _wireframeColor; }

    MAYAHYDRALIB_API
    void SetWireframeColor(const MColor& color) { _wireframeColor = color; }

    MAYAHYDRALIB_API
    GfMatrix4d GetTransform() override { return _transform[0]; }

    /// Shutter transform keys; equal to GetTransform() - which the matrix data
    /// source reads as no motion - with motion samples off. How they are made
    /// depends on MAYAHYDRA_MOTION_SAMPLING_MODE (UseVelocityMotionSamples):
    ///   evaluate (default): UpdateTransform queues a capture, and the source DAG
    ///       is evaluated at shutter open and close in one batch for all queued
    ///       items (see CaptureShutterSample). Exact, but two DG evaluations per
    ///       frame while motion samples are on.
    ///   velocity: derived when read, from the matrix's per-frame velocity (this
    ///       frame's minus the previous frame's). No DG evaluation; a straight
    ///       line between two frames, and no motion on the first frame, after a
    ///       scrub or jump, or on a frame that brought no new matrix.
    MAYAHYDRALIB_API
    GfMatrix4d GetOpenTransform() const
    {
        return UseVelocityMotionSamples() ? _ShutterTransform(true) : _transform[2];
    }

    MAYAHYDRALIB_API
    GfMatrix4d GetCloseTransform() const
    {
        return UseVelocityMotionSamples() ? _ShutterTransform(false) : _transform[1];
    }

    /// MAYAHYDRA_MOTION_SAMPLING_MODE=velocity; anything else, or unset, is
    /// evaluate. Read once.
    MAYAHYDRALIB_API
    static bool UseVelocityMotionSamples();

    /// BATCHED SHUTTER CAPTURE (evaluate mode). A transform update does not
    /// evaluate the DG at the shutter times itself: it queues a capture, and
    /// MayaHydraSceneIndex::UpdateRenderItems enters ONE MDGContextGuard per
    /// shutter time and calls CaptureShutterSample on every queued item. The
    /// upstream rig is then evaluated once per shutter time, not once per item.
    /// Until captured, the shutter keys equal the centre key (no motion).
    bool ShutterCapturePending() const { return _shutterCapturePending; }

    /// Under the guard for one shutter time: evaluate the queued DAG path's
    /// world matrix and store the open (\p open) or close key.
    MAYAHYDRALIB_API
    void CaptureShutterSample(bool open);

    /// After both samples: the capture is done.
    void FinishShutterCapture() { _shutterCapturePending = false; }

    /// True when the motion keys last published were derived from a velocity
    /// for `frame` - i.e. they show motion only as long as the frame stays
    /// `frame`. The scene index tracks these and calls DirtyStaleVelocity() on
    /// the first frame that refreshes nothing, so Storm drops the streak of an
    /// object that stopped moving. Base: the transform, velocity mode only.
    virtual bool PublishedVelocityAt(double frame) const;
    virtual void DirtyStaleVelocity();

    MAYAHYDRALIB_API
    void InvalidateTransform() { }

    MAYAHYDRALIB_API
    bool IsInstanced() const { return false; }

    MAYAHYDRALIB_API
    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override;

    MAYAHYDRALIB_API
    void UpdateTransform(const MRenderItem& ri);

    /// Class used to pass data to the UpdateFromDelta method, so we can extend the parameters in
    /// the future if needed.
    class UpdateFromDeltaData
    {
    public:
        UpdateFromDeltaData(
            MRenderItem&             ri,
            unsigned int             flags,
            const bool               wireframeColorDirty)
            : _ri(ri)
            , _flags(flags)
            , _wireframeColorDirty(wireframeColorDirty)
        {
        }

        MRenderItem&             _ri;
        unsigned int             _flags;
        const bool               _wireframeColorDirty;
    };

    /// We receive in that function the changes made in the Maya viewport between the last frame
    /// rendered and the current frame
    MAYAHYDRALIB_API
    void UpdateFromDelta(const UpdateFromDeltaData& data);

    MAYAHYDRALIB_API
    HdMeshTopology GetMeshTopology() override;

    MAYAHYDRALIB_API
    HdBasisCurvesTopology GetBasisCurvesTopology() override;

    MAYAHYDRALIB_API
    virtual TfToken GetRenderTag() const override;

    bool Illuminated() const override;

    MAYAHYDRALIB_API
    void CreateCallbacks() override;

    MAYAHYDRALIB_API
    SdfPath& GetMaterial() { return _material; }

    MAYAHYDRALIB_API
    void SetMaterial(const SdfPath& val) { _material = val; }

    MAYAHYDRALIB_API
    int GetFastID() const { return _fastId; }

    MAYAHYDRALIB_API
    const MDagPath& GetDagPath() const { return _dagPath; }

    MAYAHYDRALIB_API
    MGeometry::Primitive GetPrimitive() const { return _primitive; }

    MAYAHYDRALIB_API
    const char* Name() const { return _name.asChar(); }

    MAYAHYDRALIB_API
    void SetIsRenderITemAnaiSkydomeLightTriangleShape(bool val) {_isArnoldSkyDomeLightTriangleShape = val;}

    MAYAHYDRALIB_API
    bool GetIsRenderITemAnaiSkydomeLightTriangleShape() const {return _isArnoldSkyDomeLightTriangleShape;}

    /// Whether this item is the VP2 selection-highlight wireframe that the outline replaces.
    /// Cached because computing it needs a Maya DAG query (displayStatus()), and
    /// RefreshRenderItemLegacyHighlightTreatment reads it for every adapter. Set by
    /// UpdateRenderItems, which receives a delta for the wire whenever the shape's selection state
    /// changes. Always false while the legacy highlight is enabled.
    MAYAHYDRALIB_API
    void SetIsReplaceableHighlightWire(bool val) { _isReplaceableHighlightWire = val; }

    MAYAHYDRALIB_API
    bool GetIsReplaceableHighlightWire() const { return _isReplaceableHighlightWire; }

    /// Whether the item name is one of the VP2 selection-highlight wireframe names, the first test
    /// for GetIsReplaceableHighlightWire. A render item keeps its name for its whole life, so
    /// this is set once, when the adapter is created, instead of copying the name with
    /// MRenderItem::name() on every delta.
    MAYAHYDRALIB_API
    void SetIsSelectionHighlightWireCandidate(bool val)
    {
        _isSelectionHighlightWireCandidate = val;
    }

    MAYAHYDRALIB_API
    bool GetIsSelectionHighlightWireCandidate() const { return _isSelectionHighlightWireCandidate; }

protected:
    /// Dirty state of each vertex stream, collected over one UpdateFromDelta call.
    struct _StreamDirty
    {
        bool positions = false;
        bool normals = false;
        bool uvs = false;
        bool tangents = false;
    };

    /// Vertex stream semantics supplied by a render item's geometry.
    struct StreamPresence
    {
        bool positions = false;
        bool normals = false;
        bool uvs = false;
        bool tangents = false;
    };

    /// Scan the vertex buffers of \p geom for the semantics they supply.
    static StreamPresence _GetStreamPresence(MGeometry* geom, int vertexBufferCount);

    /// Number of values held from a previous update for the points, normals, st or
    /// tangents vertex stream.
    virtual size_t _StoredStreamCount(const TfToken& primvar) const;

    /// Called once per UpdateFromDelta, before any vertex stream is read.
    virtual void _BeginGeometryUpdate(bool /*geomChanged*/, bool /*topoChanged*/) { }

    /// Object-space bounds of the shape at \p dagPath, taken from the DAG node, for an item whose
    /// vertex data never reaches the CPU. Empty when the node has no usable bounding box.
    static GfRange3d _ObjectSpaceBoundsFromDagNode(const MDagPath& dagPath);

    /// Object-space bounds to publish, given the render item's own bounding box.
    virtual GfRange3d _ResolveBounds(const GfRange3d& renderItemBounds) const
    {
        return renderItemBounds;
    }

    /// Read one vertex stream of a changed geometry and flag it in \p dirty. \p present lists
    /// every stream the geometry supplies, including those not read yet.
    virtual void _ReadVertexStream(
        MVertexBuffer*         mvb,
        bool                   topoChanged,
        bool                   useMayaNormals,
        const StreamPresence& present,
        _StreamDirty&          dirty);

    /// Called after every vertex stream has been read, to settle the final dirty state.
    /// \p present is only meaningful when \p geomChanged or \p topoChanged is set.
    virtual void _EndGeometryUpdate(
        const StreamPresence& present,
        bool                   geomChanged,
        bool                   topoChanged,
        bool                   useMayaNormals,
        _StreamDirty&          dirty);

    /// Whether the cached topology is rebuilt from freshly-read indices.
    virtual bool
    _TopologyNeedsRebuild(bool /*emitTopologyLocators*/, bool /*hasCachedTopology*/) const
    {
        return true;
    }

    /// Single object-space normal for all vertices when geometry is a known planar (e.g. image plane).
    /// Authored as a normals primvar so renderers do not try to compute normals via GPU point readback
    /// when using GPU buffer sharing. Set at construction; empty otherwise.
    const std::optional<GfVec3f>& _ConstantNormal() const { return _constantNormal; }

    VtVec3fArray _positions = {};
    VtVec3fArray _normals = {};  //Are per vertex
    VtVec3fArray _tangents = {}; //Are face varying
    VtVec2fArray _uvs = {};      //Are face varying

private:
    MAYAHYDRALIB_API
    void _RemoveRprim();

    MAYAHYDRALIB_API
    void _InsertRprim(MayaHydraAdapter* adapter);

protected:
    // Remove and re-add this adapter's rprim under the same path, so the renderer
    // drops every buffer range it allocated for it and rebuilds from a full sync.
    // For a storage-kind change Storm cannot migrate in place (external <->
    // ordinary vertex data); see MayaHydraRenderItemOverrideAdapter.
    MAYAHYDRALIB_API
    void _ReinsertRprim();

    // Velocity mode: store this frame's matrix (all three keys) and, with
    // motion samples on, keep the previous frame's for _ShutterTransform.
    MAYAHYDRALIB_API
    void _SetTransform(const GfMatrix4d& matrix);

    MAYAHYDRALIB_API
    GfMatrix4d _ShutterTransform(bool open) const;

    // Evaluate mode: set the centre key and queue a shutter capture of \p dag,
    // whose world matrix at the current time is \p centreDag. Keys are stored as
    // world deltas applied to the centre key: key = centre * centreDag^-1 * keyDag
    // (row-vector, as Gf and Maya). With motion samples off, just the centre.
    MAYAHYDRALIB_API
    void _QueueShutterCapture(const GfMatrix4d& centre, const MDagPath& dag, const GfMatrix4d& centreDag);

    // Data is protected (as are the shutter helpers above; _RemoveRprim and
    // _InsertRprim stay private): MayaHydraRenderItemOverrideAdapter
    // (the carrier adapter) populates these from a producer-supplied payload instead of
    // from MGeometry, and then every accessor on this class - Get(),
    // GetMeshTopology(), GetBasisCurvesTopology(), GetPrimvarDescriptors(),
    // GetDisplayColor(), GetVisible(), GetTransform() - serves it unchanged.
    SdfPath                     _material;
    MDagPath                    _dagPath;
    std::unique_ptr<HdTopology> _topology = nullptr;
    MGeometry::Primitive        _primitive;
    MString                     _name;
    // [0] = shutter centre (current frame), [1] = shutter close, [2] = shutter open.
    // Identity-initialised: UpdateTransform only writes the keys when getMatrix()
    // succeeds, and comparing uninitialised keys would publish a bogus motion span.
    GfMatrix4d _transform[3] = { GfMatrix4d(1.0), GfMatrix4d(1.0), GfMatrix4d(1.0) };
    // Velocity mode (_SetTransform): the previous frame's matrix and the frames
    // of both, kept only while motion samples are on.
    GfMatrix4d _prevTransform { 1.0 };
    double     _xformFrame       = 0.0;
    double     _prevXformFrame   = 0.0;
    bool       _xformValid       = false;
    bool       _hasPrevTransform = false;
    // Evaluate mode: the queued shutter capture (see CaptureShutterSample).
    MDagPath   _shutterDag;
    GfMatrix4d _shutterCentreDagInv { 1.0 };
    bool       _shutterCapturePending = false;
    int                         _fastId = 0;
    bool                        _visible = false;
    MColor                      _wireframeColor = { 1.f, 1.f, 1.f, 1.f };
    bool                        _isHideOnPlayback = false;
    bool                        _isInPlayback = false;
    bool                        _wireframeSelectionHighlightEnabled = true;
    bool                        _isReplaceableHighlightWire = false;
    bool                        _isSelectionHighlightWireCandidate = false;
    bool                        _isArnoldSkyDomeLightTriangleShape = false;
    // See _ConstantNormal().
    std::optional<GfVec3f>      _constantNormal;
    // True when _normals holds values synthesized from _constantNormal rather than read from VP2.
    bool                        _normalsSynthesized = false;
    GfBBox3d                    _bounds;//Bounding box
    TfToken                     _purposeRenderTag;
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    MRenderItem::CullMode       _cullMode = MRenderItem::CullNone;
#endif
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H
