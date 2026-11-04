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
#ifndef MAYAHYDRA_RENDER_ITEM_OVERRIDE_ADAPTER_H
#define MAYAHYDRA_RENDER_ITEM_OVERRIDE_ADAPTER_H

#include <mayaHydraLib/adapters/renderItemAdapter.h>
#if defined(USD_HAS_GPU_BUFFER_SHARING)
#include <mayaHydraLib/adapters/mhExtGpuStreams.h>
#endif

// MayaHydraRenderItemPayload lives at GLOBAL scope (it derives from MUserData), not in
// the pxr namespace - forward-declaring it below PXR_NAMESPACE_OPEN_SCOPE would
// silently declare a different type, so include it.
#include "renderItemPayload.h"

PXR_NAMESPACE_OPEN_SCOPE

// -----------------------------------------------------------------------------
// A render item override carrier, as a render item adapter.
//
// WHY DERIVE RATHER THAN GO STRAIGHT TO THE RETAINED SCENE INDEX
// --------------------------------------------------------------
// The first carrier implementation published prims directly into the retained
// scene index and skipped the adapter registry entirely. That worked for drawing
// and quietly broke everything keyed on _renderItemsAdapters - most visibly
// PICKING: MayaViewportSceneIndex::AddPickHitToSelectionList does
//
//     auto adapter = FindAdapter<MayaHydraRenderItemAdapter>(hitId);
//     if (!adapter) { return false; }
//
// so every click on a carrier fell through with
// no diagnostic. The same lookup backs GetMaterialId, GetShadingStyle,
// GetBoundingBox, GetLightedPrimPaths and RebuildAdapterOnIdle. Re-implementing
// each of those beside the adapter was a treadmill.
//
// MayaHydraRenderItemAdapter is really two things bundled: an IDENTITY/lifecycle
// layer (the registry, the DAG path, prim insert/remove, render tags) and a
// TRANSLATION layer (reading MGeometry vertex/index buffers). A carrier only
// objects to the second. So this subclass keeps the first and replaces the
// second: it populates the base's protected geometry members straight from the
// producer's payload, after which EVERY accessor on the base - Get(),
// GetMeshTopology(), GetBasisCurvesTopology(), GetPrimvarDescriptors(),
// GetDisplayColor(), GetVisible(), GetTransform() - serves it unchanged.
//
// TWO ADAPTERS PER SHAPE, which is not a workaround but the natural fit: VP2
// makes two render items for a mesh (the shaded item and DormantPolyWire), the
// adapter registry is one-adapter-per-render-item, and Maya accordingly
// carries two render items. The wireframe item is a line list, so
// MRenderItem::primitive() reports kLines and the base class already routes it
// to a basisCurves rprim with the fallback material, constantLighting and a
// constant displayColor - the entire unlit-wireframe treatment, for free.
// -----------------------------------------------------------------------------
class MayaHydraRenderItemOverrideAdapter : public MayaHydraRenderItemAdapter
{
public:
    MayaHydraRenderItemOverrideAdapter(
        const MDagPath&      dagPath,
        const SdfPath&       slowId,
        int                  fastId,
        MayaHydraSceneIndex* mayaHydraSceneIndex,
        const MRenderItem&   ri,
        TfToken              purposeRenderTag);

    ~MayaHydraRenderItemOverrideAdapter() override = default;

    // Populate from the carrier payload instead of from MGeometry, and emit the
    // matching dirty notifications. Mirrors the shape of the base
    // UpdateFromDelta, minus everything that reads Maya geometry.
    // wireframeColorChanged is a separate argument rather than an MVS bit because
    // MVS_changedWireframeColor does not exist in every supported Maya.
    // On MVS_changedMatrix this only notifies: call UpdateRenderItemOverrideTransform
    // first, so an observer pulling the transform sees the new value.
    void UpdateFromRenderItemOverride(
        const MayaHydraRenderItemPayload& customData,
        unsigned int                           flags,
        bool                                   wireframeColorChanged);

    // A carried render item has no MRenderItem matrix, so the
    // transform comes from the DAG path the payload carries.
    void UpdateRenderItemOverrideTransform();

    // Take the payload's DAG path. The producer republishes an instanced shape's
    // carriers under the SAME ids when an instance is added or removed, so this
    // adapter outlives the path it was built with - and removing an instance
    // renumbers the others, leaving that MDagPath naming a different instance.
    void SetDagPath(const MDagPath& dagPath)
    {
        if (!(_dagPath == dagPath)) {
            _dagPath = dagPath;
        }
    }

    // The base's transform velocity, plus the points velocity SamplePrimvar
    // derives: both must be retracted once the shape stops moving.
    bool PublishedVelocityAt(double frame) const override;
    void DirtyStaleVelocity() override;

    // PER-MATERIAL GEOMSUBSETS, children of this prim, owned by the scene index
    // (MayaHydraSceneIndex::_UpdateRenderItemOverrideGeomSubsets). The count is
    // what it last inserted, so the next update can remove exactly those.
    int  GetGeomSubsetCount() const { return _geomSubsetCount; }
    void SetGeomSubsetCount(int count) { _geomSubsetCount = count; }
    // True once after the rprim was re-inserted (see
    // UpdateFromRenderItemOverride): removing a prim removes its subtree, so its
    // subsets went with it and have to be published again.
    bool ConsumeGeomSubsetsLost()
    {
        const bool lost = _geomSubsetsLost;
        _geomSubsetsLost = false;
        return lost;
    }

#if defined(USD_HAS_GPU_BUFFER_SHARING)
    // The device-resident streams of a GPU-deformed carrier, shared with the
    // renderer exactly as MayaHydraGpuRenderItemAdapter shares VP2's.
    const MhExtGpuStreams* GetExtGpuStreams() const override { return &_extStreams; }
#endif

    // DEFORMATION MOTION BLUR. Under cached playback, the shutter-open / close
    // points Maya read from the cache (exact; any frame, scrubbed or paused).
    // Otherwise shutter-open / close points extrapolated from
    // the per-frame velocity - this frame's points minus the previous frame's.
    // Deliberately NOT a re-timed evaluation (MDGContextGuard): off-context
    // skinCluster evaluation races the Evaluation Manager and intermittently
    // returns collapsed poses. Shaded payloads with CPU points only; no samples
    // on the first frame, after a scrub or jump, or on a frame that brought no
    // new points.
    size_t SamplePrimvar(const TfToken& key, size_t maxSampleCount, float* times, VtValue* samples)
        override;

protected:
    // A shared stream has no CPU array, so its element count is the published
    // one. This is also what keeps the base GetPrimvarDescriptors advertising
    // points and normals for a device-resident carrier: it only declares the
    // streams that hold data, which is why this class no longer needs its own
    // descriptor filter.
    size_t _StoredStreamCount(const TfToken& primvar) const override;

private:
    // Publish a GPU-deformed payload's device streams, or withdraw them when
    // the payload is back on CPU arrays. Returns true when the device streams
    // stand in for points and normals on this update, so the CPU arrays must
    // not be copied. \p uvsShared reports whether st is served from its device
    // copy, in which case the CPU UVs must not be either.
    bool _UpdateDeviceStreams(
        const MayaHydraRenderItemPayload& customData,
        bool                                   geomChanged,
        bool                                   topoChanged,
        bool&                                  pointsDirty,
        bool&                                  normalsDirty,
        bool&                                  uvsDirty,
        bool&                                  uvsShared);

#if defined(USD_HAS_GPU_BUFFER_SHARING)
    MhExtGpuStreams _extStreams;
#endif

    // Where the vertex data went on the last geometry update: Unknown until
    // the first one. A change of kind re-inserts the rprim - see
    // UpdateFromRenderItemOverride.
    enum class _VertexStorage : unsigned char { Unknown, Cpu, Device };
    _VertexStorage _lastVertexStorage = _VertexStorage::Unknown;

    int  _geomSubsetCount = 0;
    bool _geomSubsetsLost = false;

    // Velocity history for SamplePrimvar, kept only while motion samples are
    // on. _prevPositions is the previous frame's array (a refcount, not a
    // copy: VtArray is copy-on-write); _pointsFrame / _prevFrame are the frames
    // the two were published on.
    VtVec3fArray _prevPositions;

    // The payload's cached shutter points and the shutter offsets they were
    // built for (see SamplePrimvar); they belong to _positions. Empty when this
    // publish carried none.
    VtVec3fArray _shutterOpenPoints;
    VtVec3fArray _shutterClosePoints;
    float        _shutterOpenOffset  = 0.0f;
    float        _shutterCloseOffset = 0.0f;
    double       _pointsFrame = 0.0;
    double       _prevFrame   = 0.0;
    bool         _hasPrevPositions = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRA_RENDER_ITEM_OVERRIDE_ADAPTER_H
