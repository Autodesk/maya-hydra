//
// Copyright 2019 Luma Pictures
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
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/meshAdapterTestUtils.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/arch/hash.h>
#include <pxr/base/gf/interval.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <maya/MCallbackIdArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MNodeMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlug.h>
#include <maya/MPolyMessage.h>

#include <functional>
#include <string>
#include <unordered_set>

namespace MAYAHYDRA_NS_DEF {

namespace {

// Attributes handled by NodeDirtiedCallback or specially in AttributeChangedCallback.
// When these change, we avoid duplicate primvar dirty by returning false from
// ShouldMarkPrimvarDirtyForAttributeChange.
static const char* const kMeshParamAttributeNames[] = {
    "pnts", "inMesh", "worldMatrix", "doubleSided", "intermediateObject",
    "uvPivot", "displaySmoothMesh", "smoothLevel", "instObjGroups"
};

} // namespace

const std::unordered_set<std::string>& GetMeshParamAttributeNamesForTest()
{
    return PXR_NS::MayaHydraAdapter::GetParamAttributeSet(kMeshParamAttributeNames);
}

} // namespace MAYAHYDRA_NS_DEF

PXR_NAMESPACE_OPEN_SCOPE

/**
 * This file contains the MayaHydraMeshAdapter class to translate from a Maya mesh to hydra.
 * In the interactive viewport, mesh translation is selectable via the
 * MAYA_HYDRA_USE_MESH_ADAPTER environment variable; see MayaHydraSceneIndex::useMeshAdapter().
 *
 * We can also translate from a MRenderitem to Hydra using the
 * MayaHydraRenderItemAdapter class.
 */

namespace {

// Snapshot of mesh connectivity + a hash of point positions used to distinguish UV-only
// inMesh/pnts dirties (polyEditUV and similar) from true geometry edits. Maya often dirties
// inMesh for UV coordinate changes without firing MPolyMessage::addUVSetChangedCallback.
struct MeshGeometryState
{
    int      numVertices = -1;
    int      numPolygons = -1;
    int      numFaceVertices = -1;
    uint64_t pointsHash = 0;
    bool     valid = false;

    static uint64_t HashPoints(const GfVec3f* rawPoints, int numVertices)
    {
        if (!rawPoints || numVertices <= 0) {
            return 0;
        }
        return ArchHash64(reinterpret_cast<const char*>(rawPoints),
                          static_cast<size_t>(numVertices) * sizeof(GfVec3f));
    }

    static MeshGeometryState Capture(const MDagPath& dagPath)
    {
        MeshGeometryState state;
        MStatus           status;
        MFnMesh             mesh(dagPath, &status);
        if (!status) {
            return state;
        }
        state.numVertices = mesh.numVertices();
        state.numPolygons = mesh.numPolygons();
        state.numFaceVertices = mesh.numFaceVertices();
        const auto* rawPoints = reinterpret_cast<const GfVec3f*>(mesh.getRawPoints(&status));
        if (status && rawPoints) {
            state.pointsHash = HashPoints(rawPoints, state.numVertices);
            state.valid = true;
        }
        return state;
    }

    bool MatchesCurrent(const MDagPath& dagPath) const
    {
        if (!valid) {
            return false;
        }
        MStatus status;
        MFnMesh mesh(dagPath, &status);
        if (!status) {
            return false;
        }
        if (mesh.numVertices() != numVertices || mesh.numPolygons() != numPolygons
            || mesh.numFaceVertices() != numFaceVertices) {
            return false;
        }
        const auto* rawPoints = reinterpret_cast<const GfVec3f*>(mesh.getRawPoints(&status));
        if (!status || !rawPoints) {
            return false;
        }
        // O(n) in vertex count: scan point positions to distinguish UV-only inMesh/pnts dirties
        // from true geometry edits once O(1) connectivity counts match. This runs only on those
        // dirty notifications (not per frame or Hydra pull); treating every such dirty as a
        // geometry change would typically cost more (points, extent, subdivision, mesh re-read).
        return HashPoints(rawPoints, numVertices) == pointsHash;
    }
};

// Lambda table mapping a Maya mesh attribute to the granular dirty locators it should emit.
// Extent is dirtied conservatively on every point/topology change because the mesh adapter reads
// bounds lazily (no bbox-changed flag to diff against, unlike the render-item adapter).
using MeshDirtyFn = std::function<void(Fvp::DirtyNotifier&)>;

const std::pair<MObject&, MeshDirtyFn> _dirtyNotifiers[] {
    { MayaAttrs::mesh::worldMatrix,
      [](Fvp::DirtyNotifier& n) { n.dirtyTransform(); } },
    { MayaAttrs::mesh::doubleSided,
      [](Fvp::DirtyNotifier& n) { n.dirtyDoubleSided(); } },
    { MayaAttrs::mesh::intermediateObject,
      [](Fvp::DirtyNotifier& n) { n.dirtyVisibility(); } },
    // Tracking manual edits to uvs.
    { MayaAttrs::mesh::uvPivot,
      [](Fvp::DirtyNotifier& n) { n.dirtyPrimvar(MayaHydraAdapterTokens->st); } },
    // displaySmoothMesh and smoothLevel drive HdDisplayStyle::refineLevel via GetDisplayStyle().
    // When refineLevel transitions across 0, GetMeshTopology() flips subdivisionScheme and
    // GetSubdivTags() changes; emit displayStyle + topology + subdivisionTags only.
    { MayaAttrs::mesh::displaySmoothMesh,
      [](Fvp::DirtyNotifier& n) {
          Fvp::DirtyNotifier::DirtySmoothMeshDisplayLocators(n);
      } },
    { MayaAttrs::mesh::smoothLevel,
      [](Fvp::DirtyNotifier& n) {
          Fvp::DirtyNotifier::DirtySmoothMeshDisplayLocators(n);
      } },
};

} // namespace

/**
 * \brief MayaHydraMeshAdapter handles the translation from a Maya mesh to hydra.
 * Batch/production rendering (non-interactive) always uses this mesh adapter. In the
 * interactive viewport, the path is selectable: by default meshes are translated from
 * MRenderItems via MayaHydraRenderItemAdapter, but setting the MAYA_HYDRA_USE_MESH_ADAPTER
 * environment variable switches the viewport to use this mesh adapter instead.
 * See MayaHydraSceneIndex::useMeshAdapter().
 */
class MayaHydraMeshAdapter : public MayaHydraShapeAdapter
{
public:
    /// Create a mesh adapter for the given DAG path.
    MayaHydraMeshAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, false), mayaHydraSceneIndex, dag)
    {
    }

    /// Destroy the mesh adapter.
    ~MayaHydraMeshAdapter() = default;

    /// Insert the mesh prim into the scene index.
    void Populate() override
    {
        if (_isPopulated) {
            return;
        }
        GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->mesh, GetID());
        _isPopulated = true;
        _geometryState = MeshGeometryState::Capture(GetDagPath());
    }

    /// Track callbacks that require special removal handling.
    void AddBuggyCallback(MCallbackId id) { _buggyCallbacks.append(id); }

    /// Register Maya callbacks for mesh changes.
    void CreateCallbacks() override
    {
        MStatus status;
        auto    obj = GetNode();
        if (obj != MObject::kNullObj) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
                .Msg("Creating mesh adapter callbacks for prim (%s).\n", GetID().GetText());

            auto id
                = MNodeMessage::addNodeDirtyPlugCallback(obj, NodeDirtiedCallback, this, &status);
            if (status) {
                AddCallback(id);
            }
            id = MNodeMessage::addAttributeChangedCallback(
                obj, AttributeChangedCallback, this, &status);
            if (status) {
                AddCallback(id);
            }
            id = MPolyMessage::addPolyTopologyChangedCallback(
                obj, TopologyChangedCallback, this, &status);
            if (status) {
                AddCallback(id);
            }
            bool wantModifications[3] = { true, true, true };
            id = MPolyMessage::addPolyComponentIdChangedCallback(
                obj, wantModifications, 3, ComponentIdChanged, this, &status);
            if (status) {
                AddBuggyCallback(id);
            }
            id = MPolyMessage::addUVSetChangedCallback(obj, UVSetChangedCallback, this, &status);
            if (status) {
                AddBuggyCallback(id);
            }
        }
        MayaHydraDagAdapter::CreateCallbacks();
    }

    MAYAHYDRALIB_API
    /// Remove registered callbacks, including buggy poly callbacks.
    void RemoveCallbacks() override
    {
        if (_buggyCallbacks.length() > 0) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
                .Msg("Removing buggy PolyComponentIdChangedCallbacks\n");
            if (_node != MObject::kNullObj && MObjectHandle(_node).isValid()) {
                MMessage::removeCallbacks(_buggyCallbacks);
            }
            _buggyCallbacks.clear();
        }
        MayaHydraAdapter::RemoveCallbacks();
    }

    /// Return whether mesh prims are supported by the render index.
    bool IsSupported() const override
    {
        return GetMayaHydraSceneIndex()->IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    }

    /// Return face-varying UVs as a primvar value.
    VtValue GetUVs()
    {
        MStatus status;
        MFnMesh mesh(GetDagPath(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        //Uvs are face varying
        VtArray<GfVec2f> uvs;
        const size_t numFacesVertices = mesh.numFaceVertices();
        uvs.resize(numFacesVertices);
        float2* uvsPointer = (float2*)uvs.cdata();
        size_t numUVsFloat2 = 0;
        for (MItMeshPolygon pit(GetDagPath()); !pit.isDone(); pit.next()) {
            const auto vertexCount = pit.polygonVertexCount();
            for (auto i = decltype(vertexCount) { 0 }; i < vertexCount; ++i) {
                pit.getUV(i, *uvsPointer);
                ++uvsPointer;
                ++numUVsFloat2;
            }
        }

        if (numUVsFloat2 != numFacesVertices){
            TF_CODING_ERROR("Number of UVs does not match number of face vertices" );
        }

        return VtValue(uvs);
    }

    /// Return face-varying tangents as a primvar value.
    VtValue GetTangents()
    {
        MStatus status;
        MFnMesh mesh(GetDagPath(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        //Tangents are face varying
        const size_t numFacesVertices = mesh.numFaceVertices();
        MFloatVectorArray mayaTangents;
        mesh.getTangents(mayaTangents);
        const size_t tangentsCount = mayaTangents.length();
        if (0 == tangentsCount){
            return {};
        }

        // Tangents are declared with face-varying interpolation, so Hydra expects
        // exactly one value per face vertex. Returning a mismatched-length primvar
        // would lead to incorrect shading or downstream errors, so bail out instead.
        if (tangentsCount != numFacesVertices){
            TF_CODING_ERROR("Number of tangents (%zu) does not match number of face vertices (%zu)",
                            tangentsCount, numFacesVertices);
            return {};
        }

        VtVec3fArray ret(tangentsCount);
        for (size_t i = 0; i < tangentsCount; ++i) {
            ret[i] = GfVec3f(mayaTangents[i].x, mayaTangents[i].y, mayaTangents[i].z);
        }
        return VtValue(ret);
    }

    /// Return vertex positions as a primvar value.
    VtValue GetPoints()
    {
        MStatus status;
        MFnMesh mesh(GetDagPath(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        const auto* rawPoints = reinterpret_cast<const GfVec3f*>(mesh.getRawPoints(&status));
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }
        VtVec3fArray ret;
        ret.assign(rawPoints, rawPoints + mesh.numVertices());
        return VtValue(ret);
    }

    /// Return face-varying normals as a primvar value.
    VtValue GetNormals()
    {
        MStatus status;
        MFnMesh mesh(GetDagPath(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        MFloatVectorArray mayaNormals;
        if (mesh.getNormals(mayaNormals) != MS::kSuccess) {
            return {};
        }

        const unsigned int numFV = mesh.numFaceVertices(&status);
        if (!status) {
            return {};
        }

        // get normal indices for all vertices of faces
        MIntArray normalCounts, normalIndices;
        mesh.getNormalIds(normalCounts, normalIndices);

        VtVec3fArray ret(numFV);
        for (unsigned int i = 0; i < normalIndices.length(); ++i) {
            const MFloatVector& n = mayaNormals[normalIndices[i]];
            ret[i].Set(n.x, n.y, n.z);
        }
        return VtValue(ret);
    }

    /// Return a value for the requested Hydra data source key.
    VtValue Get(const TfToken& key) override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
            .Msg(
                "Called MayaHydraMeshAdapter::Get(%s) - %s\n",
                key.GetText(),
                GetDagPath().partialPathName().asChar());

        if (key == HdTokens->points) {
            return GetPoints();
        } 

        if (key == HdTokens->normals) {
            return GetNormals();
        } 
        
        if (key == MayaHydraAdapterTokens->tangents) {
            return GetTangents();
        }

        if (key == MayaHydraAdapterTokens->st) {
            return GetUVs();
        }

        // Let base class handle other keys
        return MayaHydraShapeAdapter::Get(key);
    }

    /// Sample time-varying primvars for the given key.
    size_t SamplePrimvar(const TfToken& key, size_t maxSampleCount, float* times, VtValue* samples)
        override
    {
        if (maxSampleCount < 1) {
            return 0;
        }

        if (key == HdTokens->points) {
            return GetMayaHydraSceneIndex()->SampleValues(
                maxSampleCount, times, samples, [&]() -> VtValue { return GetPoints(); });
        } 

        if (key == HdTokens->normals) {
            return GetMayaHydraSceneIndex()->SampleValues(
                maxSampleCount, times, samples, [&]() -> VtValue { return GetNormals(); });
        } 
        
        if (key == MayaHydraAdapterTokens->tangents) {
            times[0] = 0.0f;
            samples[0] = GetTangents();
            return 1;
        }

        if (key == MayaHydraAdapterTokens->st) {
            times[0] = 0.0f;
            samples[0] = GetUVs();
            return 1;
        }

        return 0;
    }

    /// Build the mesh topology from the Maya mesh.
    HdMeshTopology GetMeshTopology() override
    {
        MFnMesh    mesh(GetDagPath());
        const auto numPolygons = mesh.numPolygons();
        VtIntArray faceVertexCounts;
        faceVertexCounts.reserve(static_cast<size_t>(numPolygons));
        VtIntArray faceVertexIndices;
        faceVertexIndices.reserve(static_cast<size_t>(mesh.numFaceVertices()));
        for (MItMeshPolygon pit(GetDagPath()); !pit.isDone(); pit.next()) {
            const auto vc = pit.polygonVertexCount();
            faceVertexCounts.push_back(vc);
            for (auto i = decltype(vc) { 0 }; i < vc; ++i) {
                faceVertexIndices.push_back(pit.vertexIndex(i));
            }
        }

        return HdMeshTopology(
            // If no subdivision, keep custom normals set from Maya by using normal vertex buffer
            // from OGS. Otherwise, they will get overridden by catmullClark algo in Hydra.
            (GetDisplayStyle().refineLevel > 0) ? PxOsdOpenSubdivTokens->catmullClark
                                                : PxOsdOpenSubdivTokens->none,
            UsdGeomTokens->rightHanded,
            faceVertexCounts,
            faceVertexIndices);
    }

    /// Return display style based on smooth mesh settings.
    HdDisplayStyle GetDisplayStyle() override
    {
        MStatus           status;
        MFnDependencyNode node(GetNode(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return { 0, false, false };
        }
        const auto displaySmoothMesh
            = node.findPlug(MayaAttrs::mesh::displaySmoothMesh, true).asShort();
        if (displaySmoothMesh == 0) {
            return { 0, false, false };
        }
        const auto smoothLevel
            = std::max(0, node.findPlug(MayaAttrs::mesh::smoothLevel, true).asInt());
        return { smoothLevel, false, false };
    }

    /// Return subdivision tags (creases/corners) for smooth meshes.
    PxOsdSubdivTags GetSubdivTags() override
    {
        PxOsdSubdivTags tags;
        if (GetDisplayStyle().refineLevel < 1) {
            return tags;
        }

        MStatus status;
        MFnMesh mesh(GetNode(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return tags;
        }
        MUintArray   creaseVertIds;
        MDoubleArray creaseVertValues;
        mesh.getCreaseVertices(creaseVertIds, creaseVertValues);
        const auto creaseVertIdCount = creaseVertIds.length();
        if (!TF_VERIFY(creaseVertIdCount == creaseVertValues.length())) {
            return tags;
        }

        MUintArray   creaseEdgeIds;
        MDoubleArray creaseEdgeValues;
        mesh.getCreaseEdges(creaseEdgeIds, creaseEdgeValues);
        const auto creaseEdgeIdCount = creaseEdgeIds.length();
        if (!TF_VERIFY(creaseEdgeIdCount == creaseEdgeIds.length())) {
            return tags;
        }

        if (creaseVertIdCount > 0) {
            VtIntArray   cornerIndices(creaseVertIdCount);
            VtFloatArray cornerWeights(creaseVertIdCount);
            for (auto i = decltype(creaseVertIdCount) { 0 }; i < creaseVertIdCount; ++i) {
                cornerIndices[i] = static_cast<int>(creaseVertIds[i]);
                cornerWeights[i] = static_cast<float>(creaseVertValues[i]);
            }

            tags.SetCornerIndices(cornerIndices);
            tags.SetCornerWeights(cornerWeights);
        }

        if (creaseEdgeIdCount > 0) {
            VtIntArray   edgeIndices(creaseEdgeIdCount * 2);
            VtFloatArray edgeWeights(creaseEdgeIdCount);
            int          edgeVertices[2] = { 0, 0 };
            for (auto i = decltype(creaseEdgeIdCount) { 0 }; i < creaseEdgeIdCount; ++i) {
                mesh.getEdgeVertices(creaseEdgeIds[i], edgeVertices);
                edgeIndices[i * 2] = edgeVertices[0];
                edgeIndices[i * 2 + 1] = edgeVertices[1];
                edgeWeights[i] = static_cast<float>(creaseEdgeValues[i]);
            }

            tags.SetCreaseIndices(edgeIndices);
            tags.SetCreaseLengths(VtIntArray(creaseEdgeIdCount, 2));
            tags.SetCreaseWeights(edgeWeights);
        }

        tags.SetVertexInterpolationRule(UsdGeomTokens->edgeAndCorner);
        tags.SetFaceVaryingInterpolationRule(UsdGeomTokens->cornersPlus1);
        tags.SetTriangleSubdivision(UsdGeomTokens->catmullClark);

        return tags;
    }

    /// Return primvar descriptors for the requested interpolation.
    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override
    {
        // Base descriptors
        HdPrimvarDescriptorVector descs
            = MayaHydraShapeAdapter::GetPrimvarDescriptors(interpolation);

        // Local descriptors
        HdPrimvarDescriptorVector localDescs;

        if (interpolation == HdInterpolationVertex) {
            localDescs = { { UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point } };
        } else if (interpolation == HdInterpolationFaceVarying) {
            static const bool useMayaNormals = MayaHydraSceneIndex::useMayaNormals();

            if (useMayaNormals && GetDisplayStyle().refineLevel == 0) {
                localDescs.push_back(
                    { UsdGeomTokens->normals, interpolation, HdPrimvarRoleTokens->normal });
            }
            MFnMesh mesh(GetDagPath());
            if (mesh.numUVs() > 0) {
                localDescs.push_back(
                    { MayaHydraAdapterTokens->st,
                      interpolation,
                      HdPrimvarRoleTokens->textureCoordinate });
                localDescs.push_back(
                    { MayaHydraAdapterTokens->tangents,
                      interpolation,
                      HdPrimvarRoleTokens->vector });
            }
        }

        // Combine descriptors
        descs.insert(descs.end(), localDescs.begin(), localDescs.end());
        return descs;
    }

    /// Return whether the mesh is double-sided.
    bool GetDoubleSided() const override
    {
        MFnMesh mesh(GetDagPath());
        auto    p = mesh.findPlug(MayaAttrs::mesh::doubleSided, true);
        if (ARCH_UNLIKELY(p.isNull())) {
            return true;
        }
        bool doubleSided = true;
        p.getValue(doubleSided);
        return doubleSided;
    }

    /// Return whether this adapter matches the mesh Hydra type id.
    bool HasType(const TfToken& typeId) const override { return typeId == HdPrimTypeTokens->mesh; }

    /// Return the render tag for mesh geometry.
    TfToken GetRenderTag() const override { return HdRenderTagTokens->geometry; }

    /// Allow primvar dirtying for extension/dynamic mesh attributes.
    bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const override
    {
        // Always return true so AttributeChangedCallback marks primvars dirty. NodeDirtyPlugCallback
        // may not fire when resetting to default, causing primvar removal to only appear after
        // selecting away and back. Duplicate dirty notifications from NodeDirtiedCallback are idempotent.
        TF_UNUSED(plug);
        return true;
    }

private:
    /// inMesh/pnts dirties fire for both geometry edits and UV-only edits. Compare against the
    /// last captured connectivity/points to emit granular UV locators when only face-varying data
    /// changed (e.g. polyEditUV when MPolyMessage::addUVSetChangedCallback does not run).
    void DirtyInMeshOrPnts(MayaHydra::DirtyNotifier& notifier, bool useMayaNormals)
    {
        if (_geometryState.valid && _geometryState.MatchesCurrent(GetDagPath())) {
            notifier.dirtyUVs();
            return;
        }
        notifier.dirtyPoints().dirtyExtent().dirtySubdivision();
        if (useMayaNormals) {
            notifier.dirtyNormals();
        }
        _geometryState = MeshGeometryState::Capture(GetDagPath());
    }

    void RefreshGeometryState() { _geometryState = MeshGeometryState::Capture(GetDagPath()); }

    static void _NotifyConnectivityChanged(MayaHydraMeshAdapter* adapter)
    {
        MayaHydra::DirtyNotifier notifier(adapter);
        Fvp::DirtyNotifier::DirtyRprimConnectivityLocators(
            notifier, HdPrimTypeTokens->mesh);
        notifier.flush();
        adapter->RefreshGeometryState();
    }

    /// Handle node dirtied callbacks for mesh parameter changes.
    static void NodeDirtiedCallback(MObject& node, MPlug& plug, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        MStatus status;
        // For compound attrs (e.g. uvPivot), Maya fires once per child. Only process the first
        // child to avoid duplicate dirty notifications.
        if (plug.isChild()) {
            MPlug firstChild = plug.parent().child(0, &status);
            if (status && plug != firstChild) {
                return;
            }
        }
        MObject plugAttr = plug.isChild() ? plug.parent().attribute(&status) : plug.attribute(&status);
        if (!status) {
            return;
        }
        if (plugAttr == MayaAttrs::mesh::inMesh || plugAttr == MayaAttrs::mesh::pnts) {
            MayaHydra::DirtyNotifier notifier(adapter);
            adapter->DirtyInMeshOrPnts(
                notifier, MayaHydraSceneIndex::useMayaNormals());
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_PLUG_DIRTY)
                .Msg(
                    "Marking prim dirty because %s plug was dirtied.\n",
                    plug.partialName().asChar());
            return;
        }
        for (const auto& it : _dirtyNotifiers) {
            if (it.first == plugAttr) {
                // Visibility plug is not yet readable in this callback; invalidate cache so
                // GetVisible() re-queries the DAG on the next Hydra pull (see dagAdapter).
                if (plugAttr == MayaAttrs::mesh::intermediateObject) {
                    adapter->InvalidateVisibility();
                }
                MayaHydra::DirtyNotifier notifier(adapter);
                it.second(notifier);
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_PLUG_DIRTY)
                    .Msg(
                        "Marking prim dirty because %s plug was dirtied.\n",
                        plug.partialName().asChar());
                return;
            }
        }

        TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_UNHANDLED_PLUG_DIRTY)
            .Msg(
                "%s (%s) plug dirtying was not handled by "
                "MayaHydraMeshAdapter::NodeDirtiedCallback.\n",
                plug.name().asChar(),
                plug.partialName().asChar());
    }

    /// Handle attribute changes used for material assignments and primvar dirtying.
    // Extension and dynamic (user addAttr) attributes are exposed as constant primvars via
    // MayaHydraAdapter (see GetPrimvarDescriptors / GetExtensionAndDynamicAttributesFromNode).
    // We do not filter on msg here: kAttributeRemoved / kAttributeAdded (e.g. undo/redo of
    // addAttr) must reach MaybeMarkPrimvarDirtyForAttributeChange; invalid plugs during removal
    // are handled in MayaHydraAdapter::MaybeMarkPrimvarDirtyForAttributeChange.
    static void AttributeChangedCallback(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData)
    {
        TF_UNUSED(msg);
        TF_UNUSED(otherPlug);
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        if (plug == MayaAttrs::mesh::instObjGroups) {
            MayaHydra::DirtyNotifier(adapter).dirtyMaterialBinding();
        } else {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_UNHANDLED_PLUG_DIRTY)
                .Msg(
                    "%s (%s) plug dirtying was not handled by "
                    "MayaHydraMeshAdapter::attributeChangedCallback.\n",
                    plug.name().asChar(),
                    plug.partialName().asChar());
        }

        adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
    }

    /// Handle topology changes from the poly API callbacks.
    static void TopologyChangedCallback(MObject& node, void* clientData)
    {
        TF_UNUSED(node);
        _NotifyConnectivityChanged(reinterpret_cast<MayaHydraMeshAdapter*>(clientData));
    }

    /// Handle component id changes from the poly API callbacks.
    static void ComponentIdChanged(MUintArray componentIds[], unsigned int count, void* clientData)
    {
        TF_UNUSED(componentIds);
        TF_UNUSED(count);
        _NotifyConnectivityChanged(reinterpret_cast<MayaHydraMeshAdapter*>(clientData));
    }

    /// Handle UV set changes and mark primvars dirty.
    static void UVSetChangedCallback(
        MObject&                  node,
        const MString&            name,
        MPolyMessage::MessageType type,
        void*                     clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        MayaHydra::DirtyNotifier(adapter).dirtyUVs(); // granular - only the UV set changed
    }

    // Maya has a bug with removing some MPolyMessage callbacks. Known
    // problem callbacks include:
    //     MPolyMessage::addPolyComponentIdChangedCallback
    //     MPolyMessage::addUVSetChangedCallback
    // To work around this, we register these callbacks specially, and only
    // remove them if the underlying node is currently valid.
    MCallbackIdArray   _buggyCallbacks;
    MeshGeometryState _geometryState;
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraMeshAdapter, TfType::Bases<MayaHydraShapeAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, mesh)
{
    MayaHydraAdapterRegistry::RegisterShapeAdapter(
        TfToken("mesh"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag) -> MayaHydraShapeAdapterPtr {
            return MayaHydraShapeAdapterPtr(new MayaHydraMeshAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
