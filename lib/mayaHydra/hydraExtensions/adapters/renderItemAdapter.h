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

#define MAYAHYDRALIB_ENABLE_GPU_BUFFER_SHARING

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/bbox3d.h>
#include <pxr/base/tf/token.h>
#if defined(MAYAHYDRALIB_ENABLE_GPU_BUFFER_SHARING)
#include <pxr/imaging/hd/extGpuBufferSchema.h>
// MVertexBuffer and MVertexBufferDescriptor. Included explicitly rather than
// relied on transitively: MHWGeometryUtilities.h below pulls in only
// MTypes.h, so nothing else here declares them.
#include <maya/MHWGeometry.h>
#endif
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hdx/renderTask.h>
#include <pxr/pxr.h>

#include <maya/MDagPath.h>
#include <maya/MHWGeometryUtilities.h>
#include <maya/MMatrix.h>

#include <atomic>
#include <functional>
#include <memory>

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

    /// Returns the primvar's externally-owned GPU buffer as an
    /// HdExtGpuBufferSchema container, or a null handle when the primvar has
    /// no external buffer.  Consumed by MayaHydraPrimvarsDataSource, which
    /// overlays it as the primvar's `extGpuBuffer` child.
    MAYAHYDRALIB_API
    PXR_NS::HdContainerDataSourceHandle
    GetExtGpuBufferSchema(const TfToken& key) const;

    /// Materialize the CPU value corresponding to a published external GPU
    /// buffer. Called only by the lazy primvar value data source when a Hydra
    /// consumer actually pulls that value.
    MAYAHYDRALIB_API
    VtValue GetExtGpuBufferLazyValue(const TfToken& key) const;

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

    /// Shutter transform keys captured in UpdateTransform. Both equal GetTransform()
    /// when motion samples are off, which the matrix data source reads as no motion.
    MAYAHYDRALIB_API
    GfMatrix4d GetOpenTransform() const { return _transform[2]; }

    MAYAHYDRALIB_API
    GfMatrix4d GetCloseTransform() const { return _transform[1]; }

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

private:
    MAYAHYDRALIB_API
    void _RemoveRprim();

    MAYAHYDRALIB_API
    void _InsertRprim(MayaHydraAdapter* adapter);

    SdfPath                     _material;
    MDagPath                    _dagPath;
    std::unique_ptr<HdTopology> _topology = nullptr;
    VtVec3fArray                _positions = {};
    VtVec3fArray                _normals = {};//Are per vertex
    VtVec3fArray                _tangents = {}; //Are face varying
    VtVec2fArray                _uvs = {}; //Are face varying
    MGeometry::Primitive        _primitive;
    MString                     _name;
    // [0] = shutter centre (current frame), [1] = shutter close, [2] = shutter open.
    // Identity-initialised: UpdateTransform only writes the keys when getMatrix()
    // succeeds, and comparing uninitialised keys would publish a bogus motion span.
    GfMatrix4d _transform[3] = { GfMatrix4d(1.0), GfMatrix4d(1.0), GfMatrix4d(1.0) };
    int                         _fastId = 0;
    bool                        _visible = false;
    MColor                      _wireframeColor = { 1.f, 1.f, 1.f, 1.f };
    bool                        _isHideOnPlayback = false;
    bool                        _isInPlayback = false;
    bool                        _wireframeSelectionHighlightEnabled = true;
    bool                        _isReplaceableHighlightWire = false;
    bool                        _isArnoldSkyDomeLightTriangleShape = false;
    GfBBox3d                    _bounds;//Bounding box
    TfToken                     _purposeRenderTag;
#ifdef MAYA_HAS_RENDER_ITEM_CULL_MODE_API
    MRenderItem::CullMode       _cullMode = MRenderItem::CullNone;
#endif

    // GPU buffer sharing state.
    bool _UseGpuBufferSharing() const;

    enum class _ExtPublishResult
    {
        Republished, // schema (re)built: first publish, buffer change, or mode flip
        Unchanged,   // same buffer, count and mode: cached schema kept as-is
        NoGpu,       // not shareable with this renderer: caller uses the CPU path
    };

    /// One primvar stream shared out of a VP2 vertex buffer.
    struct _ExtStream
    {
        /// The HdExtGpuBufferSchema container published to the scene index as
        /// the primvar's `extGpuBuffer` child (see
        /// MayaHydraPrimvarsDataSource). Null means this stream has no shared
        /// buffer and the CPU value is authoritative.
        PXR_NS::HdContainerDataSourceHandle schema;

        /// The producer's STRONG reference to the shared buffer, and the only
        /// thing keeping it alive on our side: what travels through the scene
        /// index is weak, so that a scene-index cache outliving the geometry
        /// cannot pin GPU memory. Dropping this is how the adapter withdraws a
        /// buffer -- the arena then releases it once the GPU has retired the
        /// work that named it, not before.
        ///
        /// Held through a forward-declared type on purpose: hd declares
        /// HgiExternalBuffer without including hgi for the same reason, and
        /// shared_ptr needs no more than that to be declared, copied and
        /// destroyed.
        std::shared_ptr<PXR_NS::HgiExternalBuffer> buffer;

        /// Identity of the VP2 buffer `schema` describes, for change
        /// detection: which native buffer, how many elements, and whether we
        /// published permission to bind it directly. A byte-only deform leaves
        /// all three untouched, which is what makes it free.
        uint64_t rawHandle = 0;
        size_t   numElements = 0;
        size_t   byteOffset = 0;
        size_t   byteStride = 0;
        bool     allowDirectBind = false;

        explicit operator bool() const { return static_cast<bool>(schema); }
    };

    _ExtStream _extPositions;
    _ExtStream _extNormals;
    _ExtStream _extUvs;
    _ExtStream _extTangents;

    /// Publish one primvar stream as a shared GPU buffer, registering it with
    /// the renderer's arena and rebuilding the cached schema only when it must.
    /// Returns NoGpu, leaving \p stream untouched, when the stream cannot be
    /// shared -- no GPU handle, or a renderer that cannot consume VP2's buffers.
    _ExtPublishResult _PublishExtStream(
        MHWRender::MVertexBuffer *mvb,
        _ExtStream               &stream,
        bool                      allowDirectBind);

    bool _LazyCpuBufferTriggered(const TfToken& key) const;
    void _SetLazyCpuBufferTriggered(const TfToken& key) const;

    mutable std::atomic<uint8_t> _lazyCpuBufferTriggeredMask { 0 };

    // Hybrid-mode classifier. Starts false so the mesh runs in batch mode
    // (allowDirectBind=false, GPU-to-GPU blit). Flips to true (sticky) the first
    // time MVS_changedGeometry/MVS_changedTopo fires on a mesh that already has
    // a published buffer (filtering out the initial population frame). Once
    // true, the mesh runs in direct mode (zero-copy). Only consulted when
    // MAYAHYDRA_GPU_BUFFER_SHARING_MODE=hybrid.
    bool _classifiedAsAnimating = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_RENDER_ITEM_ADAPTER_H
