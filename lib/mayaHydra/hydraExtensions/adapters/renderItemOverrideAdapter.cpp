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
#include "renderItemOverrideAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/profilingUtils.h>

#include <maya/MAnimControl.h>
#include <maya/MMatrix.h>

#include <algorithm>
#include <maya/MViewport2Renderer.h>

#include <pxr/imaging/hd/basisCurvesTopology.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/usdGeom/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

using namespace MayaHydra;   // GetGfMatrixFromMaya, DirtyNotifier

namespace {
using MVS = MHWRender::MDataServerOperation::MViewportScene;
} // namespace

MayaHydraRenderItemOverrideAdapter::MayaHydraRenderItemOverrideAdapter(
    const MDagPath&      dagPath,
    const SdfPath&       slowId,
    int                  fastId,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MRenderItem&   ri,
    TfToken              purposeRenderTag)
    : MayaHydraRenderItemAdapter(dagPath, slowId, fastId, mayaHydraSceneIndex, ri, purposeRenderTag)
#if defined(USD_HAS_GPU_BUFFER_SHARING)
    , _extStreams(*this)
#endif
{
}

void MayaHydraRenderItemOverrideAdapter::UpdateRenderItemOverrideTransform()
{
    // A carried render item has no MRenderItem matrix, so the base
    // UpdateTransform(ri) would read garbage. The DAG path is authoritative and
    // the payload keeps it current across renames/reparents.
    if (!_dagPath.isValid()) {
        return;
    }
    const GfMatrix4d xform = GetGfMatrixFromMaya(_dagPath.inclusiveMatrix());

    // Velocity mode: the base's matrix history (GetOpenTransform), no DG
    // evaluation.
    if (UseVelocityMotionSamples()) {
        _SetTransform(xform);
        return;
    }

    // Evaluate mode, as an ordinary render item: the DAG at shutter open and
    // close, captured in the scene index's batch (CaptureShutterSample) rather
    // than with a context switch per carrier. The carrier's matrix IS the DAG
    // matrix, so the centre-relative delta reduces to the DAG key itself.
    _QueueShutterCapture(xform, _dagPath, xform);
}

bool MayaHydraRenderItemOverrideAdapter::PublishedVelocityAt(double frame) const
{
    // Points velocity (SamplePrimvar) is used in either sampling mode whenever
    // the cache supplies no shutter points, so it is tracked regardless of mode.
    if (MayaHydraRenderItemAdapter::PublishedVelocityAt(frame)) {
        return true;
    }
    return _hasPrevPositions && _pointsFrame == frame
        && GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled();
}

void MayaHydraRenderItemOverrideAdapter::DirtyStaleVelocity()
{
    MayaHydra::DirtyNotifier(this).dirtyTransform().dirtyPoints();
}

size_t MayaHydraRenderItemOverrideAdapter::SamplePrimvar(
    const TfToken& key,
    size_t         maxSampleCount,
    float*         times,
    VtValue*       samples)
{
    // Points only - what renderers sample for deformation blur; normals follow
    // the live value. Three keys at most: open, centre, close.
    if (maxSampleCount < 3 || key != HdTokens->points
        || _lastVertexStorage != _VertexStorage::Cpu) {
        return 0;
    }
    MayaHydraSceneIndex* sceneIndex = GetMayaHydraSceneIndex();
    if (!sceneIndex->GetParams().motionSamplesEnabled()) {
        return 0;
    }

    // Keys in time order; the centre key keeps an offset-0 read (a renderer
    // that does not blur) on the live points rather than on the nearer end.
    auto emit = [&](float sOpen, VtVec3fArray openPts, float sClose, VtVec3fArray closePts) {
        size_t count = 0;
        times[count]   = sOpen;
        samples[count] = VtValue(std::move(openPts));
        ++count;
        if (sOpen < 0.0f && sClose > 0.0f) {
            times[count]   = 0.0f;
            samples[count] = VtValue(_positions);
            ++count;
        }
        times[count]   = sClose;
        samples[count] = VtValue(std::move(closePts));
        ++count;
        return count;
    };

    // CACHED PLAYBACK: Maya read the shutter points from the cache, for exactly
    // these points and this shutter. Exact, and valid on any frame - scrubbed,
    // paused or the first - which velocity cannot be.
    {
        const GfInterval shutter = sceneIndex->GetCurrentTimeSamplingInterval();
        if (!_shutterOpenPoints.empty() && _shutterOpenPoints.size() == _positions.size()
            && _shutterClosePoints.size() == _positions.size()
            && _shutterOpenOffset == static_cast<float>(shutter.GetMin())
            && _shutterCloseOffset == static_cast<float>(shutter.GetMax())
            && _shutterOpenOffset < _shutterCloseOffset) {
            return emit(_shutterOpenOffset, _shutterOpenPoints, _shutterCloseOffset, _shutterClosePoints);
        }
    }

    if (!_hasPrevPositions) {
        return 0;
    }

    // These points must be THIS frame's: a frame that brought none (the mesh
    // stopped deforming) has no motion to show. And the history must be a small
    // forward step - the first frame, a scrub or a jump would otherwise turn the
    // gap into a huge bogus velocity. dF scales it to per-frame, so playback
    // stepping more than one frame still blurs by the right amount.
    // The frame from the scene index, not MAnimControl: SamplePrimvar runs during
    // Hydra sync, on Storm's worker threads.
    const double dF = _pointsFrame - _prevFrame;
    if (sceneIndex->GetCurrentFrame() != _pointsFrame || !(dF > 1e-4 && dF <= 4.0 + 1e-4)
        || _prevPositions.size() != _positions.size() || _positions.empty()) {
        return 0;
    }

    const GfInterval shutter = sceneIndex->GetCurrentTimeSamplingInterval();
    const float      sOpen   = static_cast<float>(shutter.GetMin());
    const float      sClose  = static_cast<float>(shutter.GetMax());
    if (sOpen >= sClose) {
        return 0;
    }

    const size_t       n      = _positions.size();
    const GfVec3f*     curr   = _positions.cdata();
    const GfVec3f*     prev   = _prevPositions.cdata();
    const float        invDF  = 1.0f / static_cast<float>(dF);
    VtVec3fArray       openPts(n);
    VtVec3fArray       closePts(n);
    GfVec3f*           open   = openPts.data();
    GfVec3f*           close  = closePts.data();
    float              maxDeltaSq = 0.0f;   // squared: no sqrt per vertex
    for (size_t i = 0; i < n; ++i) {
        const GfVec3f velPerFrame = (curr[i] - prev[i]) * invDF;
        open[i]  = curr[i] + velPerFrame * sOpen;
        close[i] = curr[i] + velPerFrame * sClose;
        const float d2 = (close[i] - open[i]).GetLengthSq();
        if (d2 > maxDeltaSq) {
            maxDeltaSq = d2;
        }
    }
    if (maxDeltaSq <= 1e-12f) {   // |close - open| <= 1e-6
        return 0;   // not moving: the live points stand
    }

    return emit(sOpen, std::move(openPts), sClose, std::move(closePts));
}

size_t MayaHydraRenderItemOverrideAdapter::_StoredStreamCount(const TfToken& primvar) const
{
#if defined(USD_HAS_GPU_BUFFER_SHARING)
    const size_t sharedCount = _extStreams.SharedElementCount(primvar);
    if (sharedCount > 0) {
        return sharedCount;
    }
#endif
    return MayaHydraRenderItemAdapter::_StoredStreamCount(primvar);
}

bool MayaHydraRenderItemOverrideAdapter::_UpdateDeviceStreams(
    const MayaHydraRenderItemPayload& customData,
    bool                                   geomChanged,
    bool                                   topoChanged,
    bool&                                  pointsDirty,
    bool&                                  normalsDirty,
    bool&                                  uvsDirty,
    bool&                                  uvsShared)
{
    uvsShared = false;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
    if (!customData.vertexBufferOnGPU) {
        // Back on CPU arrays - GPU Override turned off, or a sync Maya could not
        // describe the buffers for. Withdraw, so the CPU values are what the
        // primvars serve again.
        pointsDirty = _extStreams.Withdraw(UsdGeomTokens->points) || pointsDirty;
        normalsDirty = _extStreams.Withdraw(UsdGeomTokens->normals) || normalsDirty;
        uvsDirty = _extStreams.Withdraw(MayaHydraAdapterTokens->st) || uvsDirty;
        return false;
    }

    const bool isWire = customData.isLineList();
    static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();
    const bool shareNormals = !isWire && useMayaNormals && customData.deviceNormals;

    _extStreams.BeginUpdate(geomChanged, topoChanged);

    // Points also drive Hydra's normals recompute whenever Hydra owns this
    // mesh's normals, so they dirty then even under stable direct binding -
    // the same rule MayaHydraGpuRenderItemAdapter applies. Lines get no
    // normals at all.
    const bool hydraOwnsNormals = !isWire && !shareNormals;

    // Device-only: there is no CPU copy to fall back on, so a renderer that
    // cannot take the buffer still gets the values through the lazy readback.
    _extStreams.ShareDeviceOnly(
        customData.devicePoints, UsdGeomTokens->points, hydraOwnsNormals, pointsDirty);
    _positions.clear();

    if (shareNormals) {
        _extStreams.ShareDeviceOnly(
            customData.deviceNormals, UsdGeomTokens->normals, false, normalsDirty);
    } else {
        normalsDirty = _extStreams.Withdraw(UsdGeomTokens->normals) || normalsDirty;
    }
    _normals.clear();

    // UVs ride along with the deformer's buffers only so that Storm can bind
    // the prim in place: the CPU UVs are still in the payload, so a failed
    // share is no loss of data, just of in-place binding.
    if (!isWire && customData.deviceUVs
        && _extStreams.ShareDeviceOnly(
            customData.deviceUVs, MayaHydraAdapterTokens->st, false, uvsDirty)) {
        uvsShared = true;
        _uvs.clear();
    } else {
        uvsDirty = _extStreams.Withdraw(MayaHydraAdapterTokens->st) || uvsDirty;
    }
    return true;
#else
    TF_UNUSED(customData);
    TF_UNUSED(geomChanged);
    TF_UNUSED(topoChanged);
    TF_UNUSED(pointsDirty);
    TF_UNUSED(normalsDirty);
    TF_UNUSED(uvsDirty);
    return false;
#endif
}

void MayaHydraRenderItemOverrideAdapter::UpdateFromRenderItemOverride(
    const MayaHydraRenderItemPayload& customData,
    unsigned int                flags,
    bool                        wireframeColorChanged)
{
    // All line kinds (wireframe, curve, bounding box) take the same DATA path -
    // points plus an edge list, no
    // normals, no UVs. They are held apart in the payload because their meaning
    // differs (see MayaHydraRenderItemPayload::kCurve), not because the arrays do.
    const bool isWire = customData.isLineList();

    const bool geomChanged   = (flags & MVS::MVS_changedGeometry) != 0;
    const bool topoChanged   = (flags & MVS::MVS_changedTopo) != 0;
    const bool visibChanged  = (flags & MVS::MVS_changedVisibility) != 0;
    const bool matrixChanged = (flags & MVS::MVS_changedMatrix) != 0;
    const bool effectChanged = (flags & MVS::MVS_changedEffect) != 0;

    // Nothing to say. Construct the notifier only past this point so an early
    // return always leaves it empty - same discipline as the base class.
    if (!(geomChanged || topoChanged || visibChanged || matrixChanged || effectChanged
          || wireframeColorChanged)) {
        return;
    }

    MayaHydra::DirtyNotifier notifier(this);

    if (visibChanged) {
        // Per item: Maya reports visibility separately for the shaded and
        // wireframe items, so this is where "wireframe visible,
        // shaded hidden" actually lands.
        _visible = (flags & MVS::MVS_visible) != 0;
        notifier.dirtyVisibility();
    }

    if (matrixChanged) {
        // The caller already applied the transform (UpdateRenderItemOverrideTransform),
        // before the shutter capture batch is collected; only the notification is left.
        notifier.dirtyTransform();
    }

    // The producer flags topology for edits that keep the connectivity (a vertex
    // move, a UV edit, the full re-publish that builds a newly visible wire
    // shell on selection). Diff it against what was published, so neither the
    // renderer's mesh rebuild nor the velocity history reset below runs for
    // nothing. Same rule as the base, which diffs the index buffer
    // (RenderItemShouldEmitTopologyLocators). Meshes only: wires and curves
    // keep no velocity history and always rebuild their edge topology.
    bool meshConnectivityChanged = false;
    if (topoChanged && !isWire) {
        const auto* previous  = dynamic_cast<const HdMeshTopology*>(_topology.get());
        const auto  sameArray = [](const VtIntArray& a, const VtIntArray& b) {
            return a.IsIdentical(b) || a == b;
        };
        meshConnectivityChanged = !previous
            || !sameArray(previous->GetFaceVertexCounts(), customData.faceVertexCounts)
            || !sameArray(previous->GetFaceVertexIndices(), customData.faceVertexIndices);
    }

    if (geomChanged || topoChanged) {
        bool pointsDirty  = false;
        bool normalsDirty = false;
        bool uvsDirty     = false;
        bool uvsShared    = false;
        bool onDevice = false;
        {
            onDevice = _UpdateDeviceStreams(
                customData, geomChanged, topoChanged, pointsDirty, normalsDirty, uvsDirty,
                uvsShared);
        }

        // STORAGE-KIND CHANGE -> NEW RPRIM. Storm cannot move a prim's vertex
        // data between an external (aliased) range and an ordinary one in place:
        //   device -> CPU: the CPU sources are aggregated onto the aliased range,
        //       which is a CopyData coding error and draws nothing new;
        //   CPU -> device: direct bind is refused while the copied range holds a
        //       stream this sync does not replace (extGpuBufferConsumer.cpp),
        //       so the prim stays on the copy path - and pulling the device-only
        //       streams through the lazy readback latches a per-frame readback.
        // Both happen whenever the producer flips between the deformer's buffers
        // and CPU arrays: GPU Override toggled, cached playback modes that
        // evaluate on the CPU, sharing declined for a frame. Rare, so dropping
        // the rprim and letting it rebuild from a full sync is cheap and correct
        // for every cause at once. Same path, so selection and picking are kept.
        const _VertexStorage storage = onDevice ? _VertexStorage::Device : _VertexStorage::Cpu;
        if (_lastVertexStorage != _VertexStorage::Unknown && storage != _lastVertexStorage) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)
                .Msg("[GPU Buffer Sharing]: [%s] vertex data moved %s -> %s: re-inserting rprim\n",
                     GetID().GetText(),
                     onDevice ? "CPU" : "device",
                     onDevice ? "device" : "CPU");
            _ReinsertRprim();
            // The removal took the prim's geomSubset children with it.
            _geomSubsetsLost = (_geomSubsetCount > 0);
            _geomSubsetCount = 0;
#if defined(USD_HAS_GPU_BUFFER_SHARING)
            _extStreams.ResetLazyCpuConsumers();
#endif
        }
        _lastVertexStorage = storage;

        // Velocity history (see SamplePrimvar): the outgoing points become the
        // previous frame's when a new frame brings new points. Reset by anything
        // that breaks the vertex correspondence - a real connectivity change,
        // storage kind, count - and not kept at all with motion samples off.
        // Not by a topology FLAG alone: selecting a mesh re-publishes it with
        // one, and for a carrier whose blur comes from velocity (no Maya shutter
        // points, e.g. smooth mesh preview) that reset dropped the blur until two
        // more frames had played.
        const bool   motionOn = GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled();
        const double now      = motionOn ? GetMayaHydraSceneIndex()->GetCurrentFrame() : 0.0;
        if (onDevice || isWire || meshConnectivityChanged || !motionOn
            || _positions.size() != customData.points.size()) {
            _prevPositions    = VtVec3fArray();
            _hasPrevPositions = false;
        } else if (now != _pointsFrame && !_positions.empty()) {
            _prevPositions    = _positions;
            _prevFrame        = _pointsFrame;
            _hasPrevPositions = true;
        }
        // (The same frame publishing twice - a second pull - keeps its history.)

        if (!onDevice) {
            // Points are shared between the two payloads of a shape (VtArray is
            // copy-on-write), so assigning here costs a refcount for the wireframe.
            _positions = customData.points;
            _pointsFrame = now;
            if (!isWire) {
                _shutterOpenPoints  = customData.shutterOpenPoints;
                _shutterClosePoints = customData.shutterClosePoints;
                _shutterOpenOffset  = customData.shutterOpenOffset;
                _shutterCloseOffset = customData.shutterCloseOffset;
            }
            if (!isWire) {
                _normals = customData.normals;
            }
            pointsDirty = true;
            normalsDirty = normalsDirty || (!isWire && !_normals.empty());
        }
        // CPU UVs unless their device copy is published in their place.
        if (!isWire && !uvsShared) {
            // The creator re-copies uvs only when the producer says they can have
            // moved (topology, or a UV dirty detail) and otherwise keeps the same
            // array, so identity is an O(1) "changed" test. Deformation keeps them.
            uvsDirty = uvsDirty || !_uvs.IsIdentical(customData.uvs);
            _uvs = customData.uvs;
        }

        if (pointsDirty) {
            notifier.dirtyPoints();
        }
        if (normalsDirty) {
            notifier.dirtyNormals();
        }
        if (uvsDirty) {
            notifier.dirtyUVs();
        }

        // Extent: O(1). Maya computes it while it builds the points
        // (MRenderItemOverrideGeometry::bounds), precisely so this is not a
        // per-vertex loop here - the base gets it just as cheaply from
        // ri.boundingBox(), which a carried render
        // item does not have.
        // Deriving it from _positions on this thread, twice per shape (shaded +
        // wireframe) every frame, was a measurable playback regression.
        //
        // Maya has no bbox-changed flag, so diff before overwriting and only
        // dirty on a real change - same as the base.
        //
        // Device-resident data too: Maya reports the GPU deformer's own
        // bounding box for device data, and an EMPTY one only when it has
        // none. Ask the DAG only then - on a
        // GPU-deformed shape MFnDagNode::boundingBox is a CPU-side evaluation,
        // and doing it for every device payload, twice per shape, was the
        // playback hotspot of this whole path. MayaHydraGpuRenderItemAdapter
        // follows the same rule with ri.boundingBox() for VP2 items.
        GfRange3d bounds = customData.bounds;
        if (onDevice && bounds.IsEmpty()) {
            const GfRange3d dagBounds = _ObjectSpaceBoundsFromDagNode(_dagPath);
            if (!dagBounds.IsEmpty()) {
                bounds = dagBounds;
            }
        }
        if (bounds != _bounds.GetRange()) {
            _bounds.SetRange(bounds);
            notifier.dirtyExtent();
        }
        _bounds.SetMatrix(_transform[0]);
    }

    if (topoChanged) {
        if (isWire) {
            // WRAP MODE MATTERS: a curve needs a different one from the segment
            // kinds (wireframe, bounding box).
            //
            // A mesh WIREFRAME really is a bag of disconnected 2-vertex edges,
            // so `segmented` with explicit index pairs is right - and it is what
            // the base builds for a Maya kLines render item.
            //
            // A CURVE is one connected polyline. Describing it as N independent
            // segments is wrong on its own terms, and some renderers draw
            // `nonperiodic` (the wrap the base uses for a kLineStrip item) but
            // not `segmented`.
            const size_t numEdges = customData.edgeIndices.size() / 2u;

            // Only treat it as a polyline when the edge list really is the
            // chain 0-1, 1-2, 2-3, ... over exactly numEdges+1 points.
            // Maya's edge list does not promise that ordering, and emitting
            // a single polyline over an unordered edge list would draw lines
            // between unrelated vertices - worse than the wrong wrap mode.
            bool isChain = (customData.kind == MayaHydraRenderItemPayload::kCurve)
                && numEdges > 0
                && customData.edgeIndices.size() == numEdges * 2u
                && _StoredStreamCount(UsdGeomTokens->points) == numEdges + 1u;
            if (isChain) {
                for (size_t e = 0; e < numEdges; ++e) {
                    if (customData.edgeIndices[2u * e] != static_cast<int>(e)
                        || customData.edgeIndices[2u * e + 1u] != static_cast<int>(e) + 1) {
                        isChain = false;
                        break;
                    }
                }
            }

            if (isChain) {
                // Indices left EMPTY on purpose: for a single polyline Hydra
                // takes the vertex order implicitly, exactly as the base does
                // for kLineStrip. A closed curve needs no `periodic` here -
                // the producer already repeats the closing point, so an open
                // chain of numEdges+1 points describes it correctly.
                VtIntArray vertexCounts(
                    1, static_cast<int>(_StoredStreamCount(UsdGeomTokens->points)));
                _topology.reset(new HdBasisCurvesTopology(
                    HdTokens->linear,
                    {}, // basis is ignored for linear curves
                    HdTokens->nonperiodic,
                    vertexCounts,
                    VtIntArray()));
            } else {
                VtIntArray vertexCounts(numEdges, 2);
                _topology.reset(new HdBasisCurvesTopology(
                    HdTokens->linear,
                    {}, // basis is ignored for linear curves
                    HdTokens->segmented,
                    vertexCounts,
                    customData.edgeIndices));
            }
            notifier.dirtyBasisCurvesTopology();
        } else {
            // Only a real connectivity change rebuilds (see meshConnectivityChanged).
            if (meshConnectivityChanged) {
                // subdivisionScheme is always "none": the producer already handed us
                // Maya's OWN subdivided result when smooth preview is on, so letting
                // Hydra subdivide again would double-subdivide.
                _topology.reset(new HdMeshTopology(
                    PxOsdOpenSubdivTokens->none,
                    UsdGeomTokens->rightHanded,
                    customData.faceVertexCounts,
                    customData.faceVertexIndices));
                notifier.dirtyMeshTopology();
            }
        }
    }

    if (wireframeColorChanged) {
        // The colour itself is set by the caller through SetWireframeColor, from
        // MGeometryUtilities::wireframeColor - the same resolution the ordinary
        // render-item path uses, so dormant/active/lead and colour preferences
        // all match.
        notifier.dirtyDisplayColor();
    }

    if (effectChanged) {
        // REQUIRED, and easy to miss because it notifies about state this class
        // never touches. The caller has just called SetMaterial() on us; the
        // binding itself is served lazily through
        // MayaHydraSceneIndex::GetMaterialId -> _material, so without a dirty
        // nothing re-pulls it.
        //
        // And the first pull is guaranteed to be too early: UpdateRenderItems does
        // _AddRenderItem -> Populate() (which inserts the prim, and observers pull
        // straight away) BEFORE the material block runs, so the prim is first seen
        // with an empty _material and falls back to the default grey. The old
        // retained-prim design avoided this by binding the material before
        // AddPrims; on the adapter path this notification is what replaces that.
        //
        // The base class does the same thing.
        notifier.dirtyMaterialBinding();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
