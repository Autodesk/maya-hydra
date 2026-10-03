//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H
#define MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H

#include <mayaHydraLib/adapters/renderItemAdapter.h>

#include <pxr/imaging/hd/extGpuBufferSchema.h>
#include <pxr/pxr.h>

#include <maya/MHWGeometry.h>

#include <atomic>
#include <cstdint>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class Hgi;

/**
 * \brief Render item adapter that shares the VP2 vertex buffers with the renderer as
 * HdExtGpuBufferSchema primvars instead of copying them to the CPU.
 *
 * Created in place of MayaHydraRenderItemAdapter for the render items accepted by IsEligible().
 * A stream that cannot be shared on a given update falls back to the base class CPU read.
 */
class MayaHydraGpuRenderItemAdapter : public MayaHydraRenderItemAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraGpuRenderItemAdapter(
        const MDagPath&      dagPath,
        const SdfPath&       slowId,
        int                  fastId,
        MayaHydraSceneIndex* mayaHydraSceneIndex,
        const MRenderItem&   ri,
        TfToken              purposeRenderTag);

    /// Whether \p ri should be translated by this adapter: GPU buffer sharing is enabled
    /// (MAYAHYDRA_GPU_BUFFER_SHARING), the primitive maps to a mesh or basis curves, and the
    /// renderer's \p hgi can consume VP2's buffers.
    MAYAHYDRALIB_API
    static bool IsEligible(const MRenderItem& ri, Hgi* hgi);

    /// Returns the primvar's externally-owned GPU buffer as an
    /// HdExtGpuBufferSchema container, or a null handle when the primvar has
    /// no external buffer.  Consumed by MayaHydraPrimvarsDataSource, which
    /// overlays it as the primvar's `extGpuBuffer` child.
    MAYAHYDRALIB_API
    HdContainerDataSourceHandle GetExtGpuBufferSchema(const TfToken& key) const;

    /// Materialize the CPU value corresponding to a published external GPU
    /// buffer. Called only by the lazy primvar value data source when a Hydra
    /// consumer actually pulls that value.
    MAYAHYDRALIB_API
    VtValue GetExtGpuBufferLazyValue(const TfToken& key) const;

protected:
    bool      _HasStoredPositions() const override;
    size_t    _StoredPositionCount() const override;
    void      _BeginGeometryUpdate(bool geomChanged, bool topoChanged) override;
    GfRange3d _ResolveBounds(const GfRange3d& renderItemBounds) const override;
    void      _ReadVertexStream(
        MVertexBuffer* mvb,
        bool           topoChanged,
        bool           useMayaNormals,
        _StreamDirty&  dirty) override;
    void _EndGeometryUpdate(
        MGeometry*    geom,
        int           vertexBufferCount,
        bool          geomChanged,
        bool          topoChanged,
        bool          useMayaNormals,
        _StreamDirty& dirty) override;
    bool _TopologyNeedsRebuild(bool emitTopologyLocators, bool hasCachedTopology) const override;

private:
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
        HdContainerDataSourceHandle schema;

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
        std::shared_ptr<HgiExternalBuffer> buffer;

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

    /// Publish one primvar stream as a shared GPU buffer, registering it with
    /// the renderer's arena and rebuilding the cached schema only when it must.
    /// Returns NoGpu, leaving \p stream untouched, when the stream cannot be
    /// shared -- no GPU handle, or a renderer that cannot consume VP2's buffers.
    _ExtPublishResult _PublishExtStream(
        MVertexBuffer* mvb,
        _ExtStream&    stream,
        bool           allowDirectBind);

    /// Publish \p mvb into \p stream and set \p dirty when the renderer has to re-pull it.
    /// Returns false, withdrawing \p stream, when the caller must read the stream on the CPU.
    bool _ShareStream(
        MVertexBuffer* mvb,
        _ExtStream&    stream,
        const TfToken& key,
        bool           alwaysDirty,
        bool&          dirty);

    bool _LazyCpuBufferTriggered(const TfToken& key) const;
    void _SetLazyCpuBufferTriggered(const TfToken& key) const;

    _ExtStream _extPositions;
    _ExtStream _extNormals;
    _ExtStream _extUvs;
    _ExtStream _extTangents;

    mutable std::atomic<uint8_t> _lazyCpuBufferTriggeredMask { 0 };

    // Hybrid-mode classifier. Starts false so the mesh runs in batch mode
    // (allowDirectBind=false, GPU-to-GPU blit). Flips to true (sticky) the first
    // time MVS_changedGeometry/MVS_changedTopo fires on a mesh that already has
    // a published buffer (filtering out the initial population frame). Once
    // true, the mesh runs in direct mode (zero-copy). Only consulted when
    // MAYAHYDRA_GPU_BUFFER_SHARING_MODE=hybrid.
    bool _classifiedAsAnimating = false;

    // Whether the streams published on the current update allow direct binding. Decided once
    // per update in _BeginGeometryUpdate.
    bool _useDirectBind = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_GPU_RENDER_ITEM_ADAPTER_H
