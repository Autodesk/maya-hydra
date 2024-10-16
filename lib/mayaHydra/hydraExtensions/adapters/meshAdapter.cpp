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
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/gf/interval.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <maya/MCallbackIdArray.h>
#include <maya/MFloatArray.h>
#include <maya/MFnMesh.h>
#include <maya/MIntArray.h>
#include <maya/MItMeshPolygon.h>
#include <maya/MNodeMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MPlug.h>
#include <maya/MPolyMessage.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * This file contains the MayaHydraMeshAdapter class to translate from a Maya mesh to hydra.
 * Please note that, as of May 2023, this is optionally used by mayaHydra, with
 * a compile-time switch (see sceneDelegate.h).
 *
 * We can also translate from a MRenderitem to Hydra using the
 * MayaHydraRenderItemAdapter class.
 */

namespace {

const std::pair<MObject&, HdDirtyBits> _dirtyBits[] {
    { MayaAttrs::mesh::pnts,
      // This is useful when the user edits the mesh.
      HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyExtent | HdChangeTracker::DirtyNormals
          | HdChangeTracker::DirtySubdivTags },
    { MayaAttrs::mesh::inMesh,
      // We are tracking topology changes and uv changes separately
      HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyExtent | HdChangeTracker::DirtyNormals
          | HdChangeTracker::DirtySubdivTags },
    { MayaAttrs::mesh::worldMatrix, HdChangeTracker::DirtyTransform },
    { MayaAttrs::mesh::doubleSided, HdChangeTracker::DirtyDoubleSided },
    { MayaAttrs::mesh::intermediateObject, HdChangeTracker::DirtyVisibility },
    { MayaAttrs::mesh::uvPivot,
      // Tracking manual edits to uvs.
      HdChangeTracker::DirtyPrimvar },
    { MayaAttrs::mesh::displaySmoothMesh, HdChangeTracker::DirtyDisplayStyle },
    { MayaAttrs::mesh::smoothLevel, HdChangeTracker::DirtyDisplayStyle }
};

} // namespace

/**
 * \brief MayaHydraMeshAdapter is used to handle the translation from a Maya mesh to hydra.
 * Please note that, at this time, this is not used by the hydra plugin, we translate from a
 * renderitem to hydra using the MayaHydraRenderItemAdapter class.
 */
class MayaHydraMeshAdapter : public MayaHydraShapeAdapter
{
public:
    MayaHydraMeshAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, false), mayaHydraSceneIndex, dag)
    {
    }

    ~MayaHydraMeshAdapter() = default;

    void Populate() override
    {
        if (_isPopulated) {
            return;
        }
        GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->mesh, GetID());
        _isPopulated = true;
    }

    void AddBuggyCallback(MCallbackId id) { _buggyCallbacks.append(id); }

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

    bool IsSupported() const override
    {
        return GetMayaHydraSceneIndex()->GetRenderIndex().IsRprimTypeSupported(HdPrimTypeTokens->mesh);
    }

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

        if (tangentsCount != numFacesVertices){
            TF_CODING_ERROR("Number of tangents does not match number of face vertices" );
        }

       const auto* tangentsArray = reinterpret_cast<const GfVec2f*>(&mayaTangents[0]);
        VtVec2fArray ret;
        ret.assign(tangentsArray, tangentsArray + numFacesVertices);
        return VtValue(ret);
    }

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

    VtValue GetNormals()
    {
        MStatus status;
        MFnMesh mesh(GetDagPath(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        //Normals are per vertex
        MFloatVectorArray normals;
        constexpr bool angleWeighted = false;
        mesh.getVertexNormals(angleWeighted, normals);
        const auto* rawNormals = reinterpret_cast<const GfVec3f*>(&normals[0]);
        VtVec3fArray ret;
        ret.assign(rawNormals, rawNormals + mesh.numVertices());
        return VtValue(ret);
    }

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

        return {};
    }

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

        static const bool passNormalsToHydra = MayaHydraSceneIndex::passNormalsToHydra();
        return  (passNormalsToHydra) ?
            HdMeshTopology(
                        PxOsdOpenSubdivTokens->none,//For the OGS normals vertex buffer to be used, we need to use PxOsdOpenSubdivTokens->none
                        UsdGeomTokens->rightHanded,
                        faceVertexCounts,
                        faceVertexIndices) :
            HdMeshTopology(
            (GetMayaHydraSceneIndex()->GetParams().displaySmoothMeshes || GetDisplayStyle().refineLevel > 0)
                ? PxOsdOpenSubdivTokens->catmullClark
                : PxOsdOpenSubdivTokens->none,
            UsdGeomTokens->rightHanded,
            faceVertexCounts,
            faceVertexIndices);
    }

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

    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override
    {
        if (interpolation == HdInterpolationVertex) {
            static const bool passNormalsToHydra = MayaHydraSceneIndex::passNormalsToHydra();
            return  passNormalsToHydra ? 
            HdPrimvarDescriptorVector{
                {UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point},//Vertices
                {UsdGeomTokens->normals, interpolation, HdPrimvarRoleTokens->normal}//Normals
            } : 
            HdPrimvarDescriptorVector{
                {UsdGeomTokens->points, interpolation, HdPrimvarRoleTokens->point}//Vertices only
            };
        } else if (interpolation == HdInterpolationFaceVarying) {
            // UVs and tangents are face varying in maya.
            MFnMesh mesh(GetDagPath());
            if (mesh.numUVs() > 0) {
                return HdPrimvarDescriptorVector{
                    {MayaHydraAdapterTokens->st, interpolation, HdPrimvarRoleTokens->textureCoordinate},//uvs
                    {MayaHydraAdapterTokens->tangents, interpolation, HdPrimvarRoleTokens->textureCoordinate},//tangents
                };
            }
        }
        return {};
    }

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

    bool HasType(const TfToken& typeId) const override { return typeId == HdPrimTypeTokens->mesh; }

private:
    static void NodeDirtiedCallback(MObject& node, MPlug& plug, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        for (const auto& it : _dirtyBits) {
            if (it.first == plug) {
                adapter->MarkDirty(it.second);
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_PLUG_DIRTY)
                    .Msg(
                        "Marking prim dirty with bits %u because %s plug was "
                        "dirtied.\n",
                        it.second,
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

    // For material assignments for now.
    static void AttributeChangedCallback(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        if (plug == MayaAttrs::mesh::instObjGroups) {
            adapter->MarkDirty(HdChangeTracker::DirtyMaterialId);
        } else {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MESH_UNHANDLED_PLUG_DIRTY)
                .Msg(
                    "%s (%s) plug dirtying was not handled by "
                    "MayaHydraMeshAdapter::attributeChangedCallback.\n",
                    plug.name().asChar(),
                    plug.name().asChar());
        }
    }

    static void TopologyChangedCallback(MObject& node, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        adapter->MarkDirty(
            HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyPrimvar
            | HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyNormals);
    }

    static void ComponentIdChanged(MUintArray componentIds[], unsigned int count, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        adapter->MarkDirty(
            HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyPrimvar
            | HdChangeTracker::DirtyPoints);
    }

    static void UVSetChangedCallback(
        MObject&                  node,
        const MString&            name,
        MPolyMessage::MessageType type,
        void*                     clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraMeshAdapter*>(clientData);
        adapter->MarkDirty(HdChangeTracker::DirtyPrimvar);
    }

    // Maya has a bug with removing some MPolyMessage callbacks. Known
    // problem callbacks include:
    //     MPolyMessage::addPolyComponentIdChangedCallback
    //     MPolyMessage::addUVSetChangedCallback
    // Reproduction code can be found here:
    //    https://gist.github.com/elrond79/668d9809873125f608e0f7360fff7fac
    // To work around this, we register these callbacks specially, and only
    // remove them if the underlying node is currently valid.
    MCallbackIdArray _buggyCallbacks;
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
