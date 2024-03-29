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
#include "lightAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/constantShadowMatrix.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hdx/simpleLightTask.h>

#include <maya/MColor.h>
#include <maya/MFnLight.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>
#include <maya/MPoint.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraLightAdapter, TfType::Bases<MayaHydraDagAdapter>>();
}

namespace {

void _changeVisibility(
    MNodeMessage::AttributeMessage msg,
    MPlug&                         plug,
    MPlug&                         otherPlug,
    void*                          clientData)
{
    TF_UNUSED(msg);
    TF_UNUSED(otherPlug);
    if (plug == MayaAttrs::dagNode::visibility) {
        auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
        if (adapter->UpdateVisibility()) {
            adapter->RemovePrim();
            adapter->Populate();
            adapter->InvalidateTransform();
        }
    }
}

void _dirtyTransform(MObject& node, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    if (adapter->IsVisible()) {
        adapter->InvalidateTransform();
        adapter->MarkDirty(
            HdLight::DirtyTransform | HdLight::DirtyParams | HdLight::DirtyShadowParams);
    }
}

void _dirtyParams(MObject& node, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    if (adapter->IsVisible()) {
        adapter->InvalidateTransform();
        adapter->MarkDirty(HdLight::DirtyParams | HdLight::DirtyShadowParams);
    }
}

const MString defaultLightSet("defaultLightSet");

} // namespace

// MayaHydraLightAdapter is the base class for any light adapter used to handle the translation from
// a light to hydra.

MayaHydraLightAdapter::MayaHydraLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
    : MayaHydraDagAdapter(mayaHydraSceneIndex->GetPrimPath(dag, true), mayaHydraSceneIndex, dag)
{
    // This should be avoided, not a good idea to call virtual functions
    // directly or indirectly in a constructor.
    UpdateVisibility();
    _shadowProjectionMatrix.SetIdentity();
}

MayaHydraLightAdapter::~MayaHydraLightAdapter()
{
}

bool MayaHydraLightAdapter::IsSupported() const
{
    return GetMayaHydraSceneIndex()->GetRenderIndex().IsSprimTypeSupported(LightType());
}

void MayaHydraLightAdapter::Populate()
{
    if (_isPopulated) {
        return;
    }
    if (IsVisible() && _isLightingOn) {
        GetMayaHydraSceneIndex()->InsertPrim(this, LightType(), GetID());
        _isPopulated = true;
    }
}

void MayaHydraLightAdapter::MarkDirty(HdDirtyBits dirtyBits)
{
    if (_isPopulated && dirtyBits != 0) {
        GetMayaHydraSceneIndex()->MarkSprimDirty(GetID(), dirtyBits);
    }
}

void MayaHydraLightAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

bool MayaHydraLightAdapter::HasType(const TfToken& typeId) const { return typeId == LightType(); }

VtValue MayaHydraLightAdapter::Get(const TfToken& key)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg(
            "Called MayaHydraLightAdapter::Get(%s) - %s\n",
            key.GetText(),
            GetDagPath().partialPathName().asChar());

    if (key == HdLightTokens->params) {
        MFnLight       mayaLight(GetDagPath());
        GlfSimpleLight light;
        const auto     color = mayaLight.color();
        const auto     intensity = mayaLight.intensity();
        MPoint         pt(0.0, 0.0, 0.0, 1.0);
        const auto     inclusiveMatrix = GetDagPath().inclusiveMatrix();
        const auto     position = pt * inclusiveMatrix;
        // This will return zero / false if the plug is nonexistent.
        const auto decayRate
            = mayaLight.findPlug(MayaAttrs::nonAmbientLightShapeNode::decayRate, true).asShort();
        const auto emitDiffuse
            = mayaLight.findPlug(MayaAttrs::nonAmbientLightShapeNode::emitDiffuse, true).asBool();
        const auto emitSpecular
            = mayaLight.findPlug(MayaAttrs::nonAmbientLightShapeNode::emitSpecular, true).asBool();
        MVector    pv(0.0, 0.0, -1.0);
        const auto lightDirection = (pv * inclusiveMatrix).normal();
        light.SetHasShadow(false);
        const GfVec4f zeroColor(0.0f, 0.0f, 0.0f, 1.0f);
        const GfVec4f lightColor(
            color.r * intensity, color.g * intensity, color.b * intensity, 1.0f);
        light.SetDiffuse(emitDiffuse ? lightColor : zeroColor);
        light.SetAmbient(zeroColor);
        light.SetSpecular(emitSpecular ? lightColor : zeroColor);
        light.SetShadowResolution(1024);
        light.SetID(GetID());
        light.SetPosition(GfVec4f(position.x, position.y, position.z, position.w));
        light.SetSpotDirection(GfVec3f(lightDirection.x, lightDirection.y, lightDirection.z));
        if (decayRate == 0) {
            light.SetAttenuation(GfVec3f(1.0f, 0.0f, 0.0f));
        } else if (decayRate == 1) {
            light.SetAttenuation(GfVec3f(0.0f, 1.0f, 0.0f));
        } else if (decayRate == 2) {
            light.SetAttenuation(GfVec3f(0.0f, 0.0f, 1.0f));
        }
#if PXR_VERSION < 2308
        light.SetTransform(
            GetGfMatrixFromMaya(GetDagPath().inclusiveMatrixInverse()));
#else
        light.SetTransform(
            GetGfMatrixFromMaya(inclusiveMatrix));
#endif
        _CalculateLightParams(light);
        return VtValue(light);
    } else if (key == HdTokens->transform) {
        return VtValue(MayaHydraDagAdapter::GetTransform());
    } else if (key == HdLightTokens->shadowCollection) {
        // Exclude prims that should not be lighted by only
        // taking the primitives whose root path is GetMayaHydraSceneIndex()->GetLightedPrimsRootPath()
        const SdfPath     lightedPrimsRootPath = GetMayaHydraSceneIndex()->GetLightedPrimsRootPath();
        HdRprimCollection coll(
            HdTokens->geometry,
            HdReprSelector(HdReprTokens->refined),
            lightedPrimsRootPath);
        return VtValue(coll);
    } else if (key == HdLightTokens->shadowParams) {
        HdxShadowParams shadowParams;
        MFnLight        mayaLight(GetDagPath());
        const bool      bLightHasShadowsenabled = mayaLight.useRayTraceShadows();
        if (!bLightHasShadowsenabled) {
            shadowParams.enabled = false;
        } else {
            _CalculateShadowParams(mayaLight, shadowParams);
        }
        return VtValue(shadowParams);
    }
    return {};
}

VtValue MayaHydraLightAdapter::GetLightParamValue(const TfToken& paramName)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET_LIGHT_PARAM_VALUE)
        .Msg(
            "Called MayaHydraLightAdapter::GetLightParamValue(%s) - %s\n",
            paramName.GetText(),
            GetDagPath().partialPathName().asChar());

    MFnLight light(GetDagPath());
    if (paramName == HdLightTokens->color || paramName == HdTokens->displayColor) {
        const auto color = light.color();
        return VtValue(GfVec3f(color.r, color.g, color.b));
    } else if (paramName == HdLightTokens->intensity) {
        return VtValue(light.intensity());
    } else if (paramName == HdLightTokens->exposure) {
        return VtValue(0.0f);
    } else if (paramName == HdLightTokens->normalize) {
        return VtValue(true);
    } else if (paramName == HdLightTokens->enableColorTemperature) {
        return VtValue(false);
    } else if (paramName == HdLightTokens->diffuse) {
        return VtValue(light.lightDiffuse() ? 1.0f : 0.0f);
    } else if (paramName == HdLightTokens->specular) {
        return VtValue(light.lightSpecular() ? 1.0f : 0.0f);
    }
    return {};
}

void MayaHydraLightAdapter::CreateCallbacks()
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Creating light adapter callbacks for prim (%s).\n", GetID().GetText());

    MStatus status;
    auto    dag = GetDagPath();
    auto    obj = dag.node();
    auto    id = MNodeMessage::addNodeDirtyCallback(obj, _dirtyParams, this, &status);
    if (status) {
        AddCallback(id);
    }
    dag.pop();
    for (; dag.length() > 0; dag.pop()) {
        // The adapter itself will free the callbacks, so we don't have to worry
        // about passing raw pointers to the callbacks. Hopefully.
        obj = dag.node();
        if (obj != MObject::kNullObj) {
            id = MNodeMessage::addAttributeChangedCallback(obj, _changeVisibility, this, &status);
            if (status) {
                AddCallback(id);
            }
            id = MNodeMessage::addNodeDirtyCallback(obj, _dirtyTransform, this, &status);
            if (status) {
                AddCallback(id);
            }
            _AddHierarchyChangedCallbacks(dag);
        }
    }
    MayaHydraAdapter::CreateCallbacks();
}

void MayaHydraLightAdapter::SetShadowProjectionMatrix(const GfMatrix4d& matrix)
{
    if (!GfIsClose(_shadowProjectionMatrix, matrix, 0.0001)) {
        MarkDirty(HdLight::DirtyShadowParams);
        _shadowProjectionMatrix = matrix;
    }
}

void MayaHydraLightAdapter::_CalculateShadowParams(MFnLight& light, HdxShadowParams& params)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_LIGHT_SHADOWS)
        .Msg(
            "Called MayaHydraLightAdapter::_CalculateShadowParams - %s\n",
            GetDagPath().partialPathName().asChar());

    const auto dmapResolutionPlug
        = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapResolution, true);
    const auto dmapBiasPlug = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapBias, true);
    const auto dmapFilterSizePlug
        = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapFilterSize, true);

    params.enabled = true;
    params.resolution = dmapResolutionPlug.isNull()
        ? GetMayaHydraSceneIndex()->GetParams().maximumShadowMapResolution
        : std::min(
            GetMayaHydraSceneIndex()->GetParams().maximumShadowMapResolution, dmapResolutionPlug.asInt());

    params.shadowMatrix
        = std::make_shared<MayaHydraConstantShadowMatrix>(GetTransform() * _shadowProjectionMatrix);
    params.bias = dmapBiasPlug.isNull() ? -0.001 : -dmapBiasPlug.asFloat();
    params.blur = dmapFilterSizePlug.isNull() ? 0.0
                                              : (static_cast<double>(dmapFilterSizePlug.asInt()))
            / static_cast<double>(params.resolution);

    if (TfDebug::IsEnabled(MAYAHYDRALIB_ADAPTER_LIGHT_SHADOWS)) {
        std::cout << "Resulting HdxShadowParams:\n";
        std::cout << params << "\n";
    }
}

bool MayaHydraLightAdapter::_GetVisibility() const
{
    if (!GetDagPath().isVisible()) {
        return false;
    }
    // Shapes are not part of the default light set.
    if (!GetNode().hasFn(MFn::kLight)) {
        return true;
    }
    MStatus           status;
    MFnDependencyNode node(GetDagPath().transform(), &status);
    if (ARCH_UNLIKELY(!status)) {
        return true;
    }
    auto p = node.findPlug(MayaAttrs::dagNode::instObjGroups, true);
    if (ARCH_UNLIKELY(p.isNull())) {
        return true;
    }
    const auto numElements = p.numElements();
    MPlugArray conns;
    for (auto i = decltype(numElements) { 0 }; i < numElements; ++i) {
        auto ep = p[i]; // == elementByPhysicalIndex
        if (!ep.connectedTo(conns, false, true) || conns.length() < 1) {
            continue;
        }
        const auto numConns = conns.length();
        for (auto j = decltype(numConns) { 0 }; j < numConns; ++j) {
            MFnDependencyNode otherNode(conns[j].node(), &status);
            if (!status) {
                continue;
            }
            if (otherNode.name() == defaultLightSet) {
                return true;
            }
        }
    }
    return false;
}

void MayaHydraLightAdapter::SetLightingOn(bool isLightingOn)
{
    if (_isLightingOn != isLightingOn) {
        _isLightingOn = isLightingOn;

        RemovePrim();
        Populate();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
