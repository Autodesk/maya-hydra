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

#ifndef MAYAHYDRALIB_EXT_GPU_STREAMS_H
#define MAYAHYDRALIB_EXT_GPU_STREAMS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/extGpuBufferSchema.h>
#include <pxr/imaging/hd/types.h>
#include <pxr/pxr.h>

#include <maya/MHWGeometry.h>

#include <atomic>
#include <cstdint>
#include <memory>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraAdapter;

/// One vertex stream as it lives on the device: which native buffer, and where in it the
/// elements are. A plain value, so it can be kept past the call that handed out the handle --
/// which is what a producer delivering buffers ahead of the adapter update needs (render item override carriers).
struct MhExtGpuSource
{
    uint64_t    rawHandle = 0; ///< the native buffer: a GL name on OpenGL
    HdTupleType elementType = { HdTypeInvalid, 1 };
    size_t      numElements = 0;
    size_t      byteOffset = 0;
    size_t      byteStride = 0; ///< 0 means tightly packed
    /// The smallest allocation that provably contains the stream: what the consumer bounds
    /// checks the layout against.
    size_t byteSize = 0;
    /// The exact span the stream occupies, which is what may be READ out of the buffer.
    size_t copyByteSize = 0;
    /// The VP2 semantic, for the debug output. Must have static storage.
    const char* debugName = "<none>";

    /// Describe \p vertexCount elements laid out as \p descriptor in the buffer
    /// \p resourceHandle names, in the form MVertexBuffer::resourceHandle() returns it.
    /// Reads the handle out, so \p resourceHandle need not outlive the call. Returns an
    /// invalid source for a layout Hydra cannot describe.
    MAYAHYDRALIB_API
    static MhExtGpuSource FromDescriptor(
        const MHWRender::MVertexBufferDescriptor& descriptor,
        void*                                     resourceHandle,
        unsigned int                              vertexCount,
        const char*                               debugName);

    explicit operator bool() const
    {
        return rawHandle != 0 && numElements > 0 && elementType.type != HdTypeInvalid;
    }
};

/**
 * \brief The vertex streams of one adapter shared with the renderer as HdExtGpuBufferSchema
 * primvars, plus the lazy CPU readback that serves consumers which do not take the GPU buffer.
 *
 * Owned by an adapter and reached through MayaHydraAdapter::GetExtGpuStreams(), so the primvar
 * data source can publish the shared buffers without knowing which adapter produced them. The
 * adapter decides which VP2 buffer feeds which primvar; this class decides whether it can be
 * shared, (re)publishes it, and reports when the renderer has to re-pull it.
 *
 * Covers the points, normals, st and tangents primvars.
 */
class MhExtGpuStreams
{
public:
    /// \p owner names the streams in the debug output and provides the scene index whose
    /// renderer Hgi the buffers are shared with. It must outlive this object.
    MAYAHYDRALIB_API
    explicit MhExtGpuStreams(const MayaHydraAdapter& owner);

    MhExtGpuStreams(const MhExtGpuStreams&) = delete;
    MhExtGpuStreams& operator=(const MhExtGpuStreams&) = delete;

    /// Whether GPU buffer sharing is enabled (MAYAHYDRA_GPU_BUFFER_SHARING).
    MAYAHYDRALIB_API
    static bool IsEnabled();

    /// Whether the renderer that is drawing can BIND a device stream - has an
    /// extGpuBuffer bridge (Storm). One that cannot only reads device
    /// streams back, so producers should hand it CPU data instead: the readback
    /// costs the same either way, and everything that needs CPU points - motion
    /// blur samples above all - then works. Set by the render override whenever
    /// a renderer instance starts drawing; the last one to start wins when
    /// several draw at once. True until a renderer says otherwise.
    MAYAHYDRALIB_API
    static void SetRendererBindsDeviceStreams(bool binds);
    MAYAHYDRALIB_API
    static bool RendererBindsDeviceStreams();

    /// Called once per geometry update, before any stream is shared: settles whether the
    /// streams published on this update allow direct binding.
    MAYAHYDRALIB_API
    void BeginUpdate(bool geomChanged, bool topoChanged);

    /// Forget which streams a CPU consumer has pulled through the lazy readback. For when
    /// the renderer's prim was rebuilt: the new one either aliases the streams and never
    /// pulls, or pulls again and re-latches. Without this, a pull made by the old prim keeps
    /// every later update dirtied and read back for the life of the adapter.
    MAYAHYDRALIB_API
    void ResetLazyCpuConsumers();

    /// Publish \p mvb as primvar \p key and set \p dirty when the renderer has to re-pull it.
    /// \p alwaysDirty forces the dirty even when the stream is unchanged. Returns false,
    /// withdrawing a stream published earlier, when the caller must read the stream on the CPU.
    MAYAHYDRALIB_API
    bool Share(MHWRender::MVertexBuffer* mvb, const TfToken& key, bool alwaysDirty, bool& dirty);

    /// Publish \p source as primvar \p key, for a stream that has NO CPU copy to fall back on.
    /// Same as the MVertexBuffer overload, except that when the renderer cannot take the buffer
    /// the stream is still kept for the lazy CPU readback rather than withdrawn: it then has no
    /// schema, and GetLazyValue is the only way to its values. Returns false when \p source
    /// cannot be described, its element type is not Hydra's for \p key, or a bridge that
    /// exists fails to create or refresh the buffer.
    MAYAHYDRALIB_API
    bool ShareDeviceOnly(
        const MhExtGpuSource& source,
        const TfToken&        key,
        bool                  alwaysDirty,
        bool&                 dirty);

    /// Withdraw the stream published for \p key. Returns true when there was one, in which case
    /// the renderer has to re-pull the primvar.
    MAYAHYDRALIB_API
    bool Withdraw(const TfToken& key);

    /// Whether a stream is published for \p key.
    bool IsShared(const TfToken& key) const { return SharedElementCount(key) > 0; }

    /// Number of elements of the stream published for \p key, or 0 when there is none. A stream
    /// is only published with Hydra's element type, so this is the value count Hydra reads.
    /// Counts a stream kept for readback only, which has no schema.
    MAYAHYDRALIB_API
    size_t SharedElementCount(const TfToken& key) const;

    /// The HdExtGpuBufferSchema container published for \p key, or null -- also for a stream
    /// kept for readback only.
    MAYAHYDRALIB_API
    HdContainerDataSourceHandle GetSchema(const TfToken& key) const;

    /// Materialize the CPU value of the stream published for \p key. Called only by the lazy
    /// primvar value data source, when a Hydra consumer actually pulls that value.
    MAYAHYDRALIB_API
    VtValue GetLazyValue(const TfToken& key) const;

private:
    enum class _PublishResult
    {
        Republished, // schema (re)built: first publish, buffer change, or mode flip
        Unchanged,   // same buffer, count and mode: cached schema kept as-is
        NoGpu,       // not shareable with this renderer: caller uses the CPU path
        ReadbackOnly, // no bridge, kept without a schema for the lazy CPU readback
    };

    /// One primvar stream shared out of a VP2 vertex buffer.
    struct _Stream
    {
        /// The HdExtGpuBufferSchema container published to the scene index as
        /// the primvar's `extGpuBuffer` child (see
        /// MayaHydraPrimvarsDataSource). Null means no renderer can bind this
        /// stream: the CPU value is authoritative, unless `readbackOnly`, in
        /// which case the lazy readback is.
        HdContainerDataSourceHandle schema;

        /// The producer's STRONG reference to the shared buffer, and the only
        /// thing keeping it alive on our side: what travels through the scene
        /// index is weak, so that a scene-index cache outliving the geometry
        /// cannot pin GPU memory. Dropping this is how a stream is withdrawn
        /// (Withdraw, _ShareFailed) -- the arena then releases it once the GPU has retired the
        /// work that named it, not before.
        ///
        /// Held through a forward-declared type on purpose: hd declares
        /// HgiExternalBuffer without including hgi for the same reason, and
        /// shared_ptr needs no more than that to be declared, copied and
        /// destroyed.
        std::shared_ptr<HgiExternalBuffer> buffer;

        /// Identity of the VP2 buffer `schema` describes, for change
        /// detection: which native buffer, its layout, and whether we published
        /// permission to bind it directly (the native buffer counts only on a
        /// zero-copy bridge). A byte-only deform leaves all of them untouched,
        /// which is what makes it free.
        uint64_t    rawHandle = 0;
        size_t      numElements = 0;
        size_t      byteOffset = 0;
        size_t      byteStride = 0;
        HdTupleType tupleType = { HdTypeInvalid, 1 };
        bool        allowDirectBind = false;

        /// Published with no schema: the renderer cannot take the buffer, and the stream has no
        /// CPU copy, so the lazy readback is the only way to its values.
        bool readbackOnly = false;

        explicit operator bool() const { return static_cast<bool>(schema) || readbackOnly; }
    };

    /// Publish one primvar stream as a shared GPU buffer, registering it with
    /// the renderer's arena and rebuilding the cached schema only when it must.
    /// Returns NoGpu, leaving \p stream untouched, when the stream cannot be
    /// shared -- an element type other than Hydra's for \p key, no bridge to the renderer's
    /// Hgi, or a bridge that fails to create or refresh the buffer.
    /// With \p deviceOnly, a renderer that cannot take the buffer yields ReadbackOnly rather
    /// than NoGpu.
    _PublishResult _Publish(
        const MhExtGpuSource& source,
        _Stream&              stream,
        const TfToken&        key,
        bool                  allowDirectBind,
        bool                  deviceOnly);

    /// Withdraw \p stream when it was published, setting \p dirty. Always returns false.
    bool _ShareFailed(_Stream& stream, bool& dirty);

    /// Settle \p dirty and queue the prefetch after a successful publish.
    void _AfterPublish(_PublishResult res, _Stream& stream, const TfToken& key, bool alwaysDirty,
                       bool& dirty);

    /// The stream for the points, normals, st or tangents primvar, or null.
    _Stream*       _StreamFor(const TfToken& key);
    const _Stream* _StreamFor(const TfToken& key) const;

    bool _LazyCpuBufferTriggered(const TfToken& key) const;
    void _SetLazyCpuBufferTriggered(const TfToken& key) const;

    const MayaHydraAdapter& _owner;

    _Stream _positions;
    _Stream _normals;
    _Stream _uvs;
    _Stream _tangents;

    mutable std::atomic<uint8_t> _lazyCpuBufferTriggeredMask { 0 };

    // Hybrid-mode classifier. Starts false so the mesh runs in batch mode
    // (allowDirectBind=false, GPU-to-GPU blit). Flips to true (sticky) on the
    // second geometry-only change (MVS_changedGeometry without MVS_changedTopo)
    // since the mesh was last (re)populated - i.e. a deform. VP2 sends one such
    // change after populating a rigid mesh (e.g. after a display-mode toggle), so
    // one is not enough. Once true, the mesh runs in direct mode (zero-copy).
    // Only consulted when MAYAHYDRA_GPU_BUFFER_SHARING_MODE=hybrid.
    bool _classifiedAsAnimating = false;
    // Geometry-only changes since the last topology change, while unclassified in
    // hybrid mode; saturates at the promotion threshold.
    int _geomOnlyUpdatesSinceTopo = 0;

    // Whether the streams published on the current update allow direct binding. Decided once
    // per update in BeginUpdate.
    bool _useDirectBind = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_EXT_GPU_STREAMS_H
