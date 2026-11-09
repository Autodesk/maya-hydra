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

#include "gpuRenderItemAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mhExtGpuBufferBridge.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/profilingUtils.h>

#include <pxr/base/tf/registryManager.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hgi/hgi.h>

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

#define TF_DEBUG_GPU_BUFFER_SHARING(fmt, ...)                      \
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)      \
        .Msg("[GPU Buffer Sharing]: " fmt, ##__VA_ARGS__)

namespace {

// The VP2 semantic of a stream, for log lines that need to say WHICH
// primvar was or was not shared.
//
// Mapped from the enum rather than taken from
// MVertexBufferDescriptor::semanticName(), which returns MString BY VALUE:
// asChar() on that temporary would dangle the moment this function
// returned. String literals have static storage, so these do not.
const char* _ExtStreamName(MHWRender::MVertexBuffer* mvb)
{
    if (!mvb) {
        return "<none>";
    }
    switch (mvb->descriptor().semantic()) {
    case MGeometry::Semantic::kPosition: return "position";
    case MGeometry::Semantic::kNormal: return "normal";
    case MGeometry::Semantic::kTexture: return "texture";
    case MGeometry::Semantic::kColor: return "color";
    case MGeometry::Semantic::kTangent: return "tangent";
    case MGeometry::Semantic::kBitangent: return "bitangent";
    case MGeometry::Semantic::kTangentWithSign: return "tangentWithSign";
    case MGeometry::Semantic::kInvalidSemantic: return "<invalid>";
    }
    return "<unknown>";
}

} // namespace

MayaHydraGpuRenderItemAdapter::MayaHydraGpuRenderItemAdapter(
    const MDagPath&      dagPath,
    const SdfPath&       slowId,
    int                  fastId,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MRenderItem&   ri,
    TfToken              purposeRenderTag)
    : MayaHydraRenderItemAdapter(
        dagPath, slowId, fastId, mayaHydraSceneIndex, ri, purposeRenderTag)
    , _extStreams(*this)
{
}

bool MayaHydraGpuRenderItemAdapter::IsEligible(const MRenderItem& ri, Hgi* hgi)
{
    if (!MhExtGpuStreams::IsEnabled()) {
        return false;
    }
    // Triangles/strips -> HdMesh, lines/line-strips -> HdBasisCurves. Points
    // are intentionally excluded: point render items are not populated by
    // UpdateFromDelta yet (see its early-exit guard), so there is nothing to
    // share.
    switch (ri.primitive()) {
    case MGeometry::Primitive::kTriangles:
    case MGeometry::Primitive::kTriangleStrip:
    case MGeometry::Primitive::kLines:
    case MGeometry::Primitive::kLineStrip: break;
    default:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] Render item -> CPU: primitive type %d is not eligible for GPU buffer sharing\n",
            ri.name().asChar(), int(ri.primitive()));
        return false;
    }
    // A VP2 that is not on GL, or a renderer Hgi with no arena to share through (e.g. Metal),
    // can never share a stream, so such items stay on the CPU path entirely.
    const bool bridgeAvailable = static_cast<bool>(MhExtGpuBufferBridge::ForHgi(hgi));
    if (!bridgeAvailable) {
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] Render item -> CPU: renderer Hgi (%s) has no GPU buffer sharing bridge\n",
            ri.name().asChar(),
            hgi ? hgi->GetAPIName().GetText() : "<null>");
        return false;
    }
    return true;
}

size_t MayaHydraGpuRenderItemAdapter::_StoredStreamCount(const TfToken& primvar) const
{
    // A shared stream's CPU array is cleared once it is published, so the published
    // external buffer is the baseline.
    const size_t sharedCount = _extStreams.SharedElementCount(primvar);
    return sharedCount > 0 ? sharedCount : MayaHydraRenderItemAdapter::_StoredStreamCount(primvar);
}

void MayaHydraGpuRenderItemAdapter::_BeginGeometryUpdate(bool geomChanged, bool topoChanged)
{
    _extStreams.BeginUpdate(geomChanged, topoChanged);
}

GfRange3d MayaHydraGpuRenderItemAdapter::_ResolveBounds(const GfRange3d& renderItemBounds) const
{
    // GPU-buffer render items often have an empty MRenderItem bbox (vertex
    // data is GPU-only); fall back to the source shape's DAG-node bounds so
    // the prim isn't culled.
    return renderItemBounds.IsEmpty() ? _ObjectSpaceBoundsFromDagNode(GetDagPath())
                                      : renderItemBounds;
}

void MayaHydraGpuRenderItemAdapter::_ReadVertexStream(
    MVertexBuffer*         mvb,
    bool                   topoChanged,
    bool                   useMayaNormals,
    const StreamPresence& present,
    _StreamDirty&          dirty)
{
    MH_PROFILE_FUNCTION();

    const bool isMesh = GetPrimitive() == MGeometry::Primitive::kTriangles
        || GetPrimitive() == MGeometry::Primitive::kTriangleStrip;
    switch (mvb->descriptor().semantic()) {
    case MGeometry::Semantic::kPosition: {
        // Points also drive Hydra's normals recompute whenever Hydra owns this item's normals --
        // Maya normals disabled, or a mesh whose shader requested no normal stream -- so they
        // dirty then even under stable direct binding. Lines and curves never carry a normal
        // stream and Hydra computes none for them, so a missing stream does not count there.
        const bool hydraOwnsNormals = !useMayaNormals || (isMesh && !present.normals);
        if (_extStreams.Share(mvb, HdTokens->points, hydraOwnsNormals, dirty.positions)) {
            _positions.clear();
            return;
        }
        break;
    }
    case MGeometry::Semantic::kNormal:
        if (useMayaNormals
            && _extStreams.Share(mvb, HdTokens->normals, false, dirty.normals)) {
            _normals.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTexture:
        if (isMesh && _extStreams.Share(mvb, MayaHydraAdapterTokens->st, false, dirty.uvs)) {
            _uvs.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kTangent:
        if (isMesh
            && _extStreams.Share(
                mvb, MayaHydraAdapterTokens->tangents, false, dirty.tangents)) {
            _tangents.clear();
            return;
        }
        break;
    case MGeometry::Semantic::kColor:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: vertex colors not supported on GPU path yet\n",
            GetID().GetText(), _ExtStreamName(mvb));
        break;
    default:
        TF_DEBUG_GPU_BUFFER_SHARING(
            "[%s] %s -> CPU: semantic %d not shared\n",
            GetID().GetText(), _ExtStreamName(mvb), int(mvb->descriptor().semantic()));
        break;
    }

    TF_DEBUG_GPU_BUFFER_SHARING(
        "[%s] %s falling back to CPU MVertexBuffer::map() read\n",
        GetID().GetText(), _ExtStreamName(mvb));

    MayaHydraRenderItemAdapter::_ReadVertexStream(mvb, topoChanged, useMayaNormals, present, dirty);
}

void MayaHydraGpuRenderItemAdapter::_EndGeometryUpdate(
    const StreamPresence& present,
    bool                   geomChanged,
    bool                   topoChanged,
    bool                   useMayaNormals,
    _StreamDirty&          dirty)
{
    // A stream published earlier but absent from this geometry must be
    // withdrawn. VP2 can release a hidden item's buffers (a display-mode
    // switch does) and recycle the names, and the stream reads only visit the
    // buffers that are still there -- so without this the cached schema keeps
    // naming a GL buffer VP2 no longer guarantees, and Storm copies from or
    // draws through whatever that name now is.
    if (geomChanged || topoChanged) {
        auto withdraw = [this](const TfToken& key, bool isPresent, bool& streamDirty) {
            if (!isPresent && _extStreams.Withdraw(key)) {
                streamDirty = true;
            }
        };
        withdraw(HdTokens->points, present.positions, dirty.positions);
        withdraw(HdTokens->normals, present.normals, dirty.normals);
        withdraw(MayaHydraAdapterTokens->st, present.uvs, dirty.uvs);
        withdraw(MayaHydraAdapterTokens->tangents, present.tangents, dirty.tangents);
    }

    // Streams read on this update are already dirty. A stream VP2 did not supply has nothing
    // to re-pull on a deformation-only frame (playback sets geomChanged without topoChanged); it
    // can only have appeared or changed when Maya flags a topology change.
    if (geomChanged && topoChanged) {
        dirty.positions = dirty.positions || !_extStreams.IsShared(HdTokens->points);
        dirty.uvs = dirty.uvs || !_extStreams.IsShared(MayaHydraAdapterTokens->st);
        dirty.tangents
            = dirty.tangents || !_extStreams.IsShared(MayaHydraAdapterTokens->tangents);
        dirty.normals
            = dirty.normals || (useMayaNormals && !_extStreams.IsShared(HdTokens->normals));
    }
}

bool MayaHydraGpuRenderItemAdapter::_TopologyNeedsRebuild(
    bool emitTopologyLocators,
    bool hasCachedTopology) const
{
    // Only (re)build the cached topology when connectivity actually changed
    // (== emitTopologyLocators, which already ran the connectivity diff) or when there is no
    // cached topology yet. Maya raises MVS_changedGeometry every frame during deformation with
    // unchanged connectivity; rebuilding on those frames would rescan the whole face-vertex index
    // array (HdMeshTopology::ComputeNumPoints) and reallocate for nothing - no topology-dirty
    // locator is emitted on those frames, so Storm never re-pulls the rebuilt copy.
    return emitTopologyLocators || !hasCachedTopology;
}

///////////////////////////////////////////////////////////////////////
// TF_REGISTRY
///////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraGpuRenderItemAdapter, TfType::Bases<MayaHydraRenderItemAdapter>>();
}

#undef TF_DEBUG_GPU_BUFFER_SHARING

PXR_NAMESPACE_CLOSE_SCOPE
