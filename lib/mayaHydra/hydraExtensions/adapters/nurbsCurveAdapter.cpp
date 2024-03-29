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
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/pxOsd/tokens.h>
#include <pxr/pxr.h>
#include <pxr/usd/usdGeom/tokens.h>

#include <maya/MFnNurbsCurve.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MPointArray.h>
#include <maya/MPolyMessage.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * This file contains the MayaHydraNurbsCurveAdapter class to translate from a Maya NURBS curve to
 * hydra. Please note that, at this time, this is not used by the hydra plugin, we translate from a
 * renderitem to hydra using the MayaHydraRenderItemAdapter class.
 */

namespace {

const std::array<std::pair<MObject&, HdDirtyBits>, 4> _dirtyBits { {
    { MayaAttrs::nurbsCurve::controlPoints,
      HdChangeTracker::DirtyPoints | HdChangeTracker::DirtyExtent },
    { MayaAttrs::nurbsCurve::worldMatrix, HdChangeTracker::DirtyTransform },
    { MayaAttrs::nurbsCurve::doubleSided, HdChangeTracker::DirtyDoubleSided },
    { MayaAttrs::nurbsCurve::intermediateObject, HdChangeTracker::DirtyVisibility },
} };

} // namespace

/**
 * \brief MayaHydraNurbsCurveAdapter is used to handle the translation from a Maya NURBS curve to
 * hydra. Please note that, at this time, this is not used by the hydra plugin, we translate from a
 * renderitem to hydra using the MayaHydraRenderItemAdapter class.
 */
class MayaHydraNurbsCurveAdapter : public MayaHydraShapeAdapter
{
public:
    MayaHydraNurbsCurveAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, false), mayaHydraSceneIndex, dag)
    {
    }

    ~MayaHydraNurbsCurveAdapter() = default;

    bool IsSupported() const override
    {
        return GetMayaHydraSceneIndex()->GetRenderIndex().IsRprimTypeSupported(HdPrimTypeTokens->basisCurves);
    }

    void Populate() override { GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->basisCurves, GetID()); }

    void CreateCallbacks() override
    {
        MStatus status;
        auto    obj = GetNode();
        if (obj != MObject::kNullObj) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
                .Msg("Creating nurbs curve adapter callbacks for prim (%s).\n", GetID().GetText());

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
                AddCallback(id);
            }
        }
        MayaHydraDagAdapter::CreateCallbacks();
    }

    VtValue Get(const TfToken& key) override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
            .Msg(
                "Called MayaHydraNurbsCurveAdapter::Get(%s) - %s\n",
                key.GetText(),
                GetDagPath().partialPathName().asChar());

        if (key == HdTokens->points) {
            MFnNurbsCurve curve(GetDagPath());
            MStatus       status;
            MPointArray   pointArray;
            status = curve.getCVs(pointArray);
            if (!status) {
                return {};
            }
            VtVec3fArray ret(pointArray.length());
            const auto   pointCount = pointArray.length();
            for (auto i = decltype(pointCount) { 0 }; i < pointCount; i++) {
                const auto pt = pointArray[i];
                ret[i] = GfVec3f(
                    static_cast<float>(pt.x), static_cast<float>(pt.y), static_cast<float>(pt.z));
            }
            return VtValue(ret);
        }
        return {};
    }

    HdBasisCurvesTopology GetBasisCurvesTopology() override
    {
        MFnNurbsCurve curve(GetDagPath());
        const auto    pointCount = curve.numCVs();

        VtIntArray curveVertexCounts;
        const auto numIndices = (pointCount - 1) * 2;
        curveVertexCounts.push_back(numIndices);
        VtIntArray curveIndices(static_cast<size_t>(numIndices));
        for (auto i = decltype(numIndices) { 0 }; i < numIndices / 2; i++) {
            curveIndices[i * 2] = i;
            curveIndices[i * 2 + 1] = i + 1;
        }

        return HdBasisCurvesTopology(
            HdTokens->linear,
            HdTokens->bezier,
            HdTokens->segmented,
            curveVertexCounts,
            curveIndices);
    }

    HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation) override
    {
        if (interpolation == HdInterpolationVertex) {
            HdPrimvarDescriptor desc;
            desc.name = UsdGeomTokens->points;
            desc.interpolation = interpolation;
            desc.role = HdPrimvarRoleTokens->point;
            return { desc };
        }
        return {};
    }

    TfToken GetRenderTag() const override { return HdRenderTagTokens->guide; }

private:
    static void NodeDirtiedCallback(MObject& node, MPlug& plug, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraNurbsCurveAdapter*>(clientData);
        for (const auto& it : _dirtyBits) {
            if (it.first == plug) {
                adapter->MarkDirty(it.second);
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_CURVE_PLUG_DIRTY)
                    .Msg(
                        "Marking prim dirty with bits %u because %s plug was "
                        "dirtied.\n",
                        it.second,
                        plug.partialName().asChar());
                return;
            }
        }

        TF_DEBUG(MAYAHYDRALIB_ADAPTER_CURVE_UNHANDLED_PLUG_DIRTY)
            .Msg(
                "%s (%s) plug dirtying was not handled by "
                "MayaHydraNurbsCurveAdapter::NodeDirtiedCallback.\n",
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
        auto* adapter = reinterpret_cast<MayaHydraNurbsCurveAdapter*>(clientData);
        if (plug == MayaAttrs::mesh::instObjGroups) {
            adapter->MarkDirty(HdChangeTracker::DirtyMaterialId);
        } else {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_CURVE_UNHANDLED_PLUG_DIRTY)
                .Msg(
                    "%s (%s) plug dirtying was not handled by "
                    "MayaHydraNurbsCurveAdapter::attributeChangedCallback.\n",
                    plug.name().asChar(),
                    plug.name().asChar());
        }
    }

    static void TopologyChangedCallback(MObject& node, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraNurbsCurveAdapter*>(clientData);
        adapter->MarkDirty(
            HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyPrimvar
            | HdChangeTracker::DirtyPoints);
    }

    static void ComponentIdChanged(MUintArray componentIds[], unsigned int count, void* clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraNurbsCurveAdapter*>(clientData);
        adapter->MarkDirty(
            HdChangeTracker::DirtyTopology | HdChangeTracker::DirtyPrimvar
            | HdChangeTracker::DirtyPoints);
    }
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraNurbsCurveAdapter, TfType::Bases<MayaHydraShapeAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, mesh)
{
    MayaHydraAdapterRegistry::RegisterShapeAdapter(
        TfToken("nurbsCurve"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag) -> MayaHydraShapeAdapterPtr {
            return MayaHydraShapeAdapterPtr(new MayaHydraNurbsCurveAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
