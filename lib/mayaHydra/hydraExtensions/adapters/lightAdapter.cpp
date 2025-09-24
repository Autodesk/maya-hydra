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
#include <mayaHydraLib/mayaUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hdx/simpleLightTask.h>
#include <pxr/usd/usdLux/tokens.h>

#include <maya/MColor.h>
#include <maya/MFnLight.h>
#include <maya/MFnSpotLight.h>
#include <maya/MFnPointLight.h>
#include <maya/MFnAreaLight.h>
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

    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    if (plug == MayaAttrs::dagNode::visibility) {
        if (adapter->UpdateVisibility()) {
            adapter->RemovePrim();
            adapter->Populate();
            adapter->InvalidateTransform();
        }
    }

    // Handle extension attributes change
    adapter->HandleExtensionAttributesDirty(plug);
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

GfMatrix4d MayaHydraLightAdapter::_CalculateShadowProjectionMatrix()
{
    // Calculate the shadow projection matrix based on the light type and its parameters.
    // This is similar to how Maya VP2 calculates internally.
    MFnLight light(GetDagPath());
    const auto xform = GetTransform();
    auto pos = xform.Transform(GfVec3d(0.0, 0.0, 0.0)); // Maya light pos is (0,0,0) by default
    auto dir = xform.TransformDir(GfVec3d(0.0, 0.0, -1.0)); // Maya light dir is -Z by default
    auto up = xform.TransformDir(GfVec3d(0.0, 1.0, 0.0));  // Maya light up is +Y by default

    auto isDirectional = GetNode().hasFn(MFn::kDirectionalLight);
    auto useDmapAutoFocus = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::useDmapAutoFocus, true).asBool();
    auto dmapWidthFocus = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapWidthFocus, true).asDouble(); 
    auto farClip = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapFarClipPlane, true).asDouble();
    auto nearClip = light.findPlug(MayaAttrs::nonExtendedLightShapeNode::dmapNearClipPlane, true).asDouble();

    // View matrix is also needed as the light position needs to be adjusted to fit the scene.
    // To make sure the adjusted view matrix takes effect, we'll multiply the calculated matrix by the light transform, 
    // this will give the expected final matrix when Hydra is computing the ViewProjection matrix for shadow map.
    // Hydra ViewProjection Matrix = ViewMatrix(inverted LightTransform) * ProjectionMatrix(LightTransform * viewMatrix * projectionMatrix)
    // = viewMatrix * projectionMatrix.
    GfMatrix4d viewMatrix;
    GfMatrix4d projMatrix;
    if (isDirectional) {
        // Adjust light position to fit the scene
        GfBBox3d    bbox = GetMayaHydraSceneIndex()->GetBoundingBox();
        GfVec3d     boxCenter = bbox.ComputeCentroid();
        GfBBox3d    aabb = bbox.ComputeAlignedBox();
        const GfRange3d& r = aabb.GetRange();
        auto        boxRadius = 0.5 * (r.GetMax() - r.GetMin()).GetLength();
        pos = boxCenter - boxRadius * dir;

        // View matrix
        GfVec3d    ndir = dir.GetNormalized();
        GfVec3d    right = GfCross(up, ndir).GetNormalized();
        GfVec3d    realUp = GfCross(ndir, right);
        viewMatrix.SetColumn(0, GfVec4d(right[0], right[1], right[2], -GfDot(right, pos)));
        viewMatrix.SetColumn(1, GfVec4d(realUp[0], realUp[1], realUp[2], -GfDot(realUp, pos)));
        viewMatrix.SetColumn(2, GfVec4d(ndir[0], ndir[1], ndir[2], -GfDot(ndir, pos)));
        viewMatrix.SetColumn(3, GfVec4d(0,0,0,1));

        // Proj matrix
        auto frustumDepth = 2.0 * boxRadius;
        frustumDepth = frustumDepth <= 0.00001 ? 0.00001 : frustumDepth;
        auto fov = useDmapAutoFocus ? frustumDepth : dmapWidthFocus;

        projMatrix.SetIdentity();
        projMatrix[0][0] = 2.0 / fov;
        projMatrix[1][1] = 2.0 / fov;
        projMatrix[2][2] = 1.0 / frustumDepth;
        projMatrix[3][2] = 0.0;
        projMatrix[2][3] = 0.0;
        projMatrix[3][3] = 1.0;
    } else {
        // View matrix
        GfVec3d ndir = dir.GetNormalized();
        GfVec3d right = GfCross(up, ndir).GetNormalized();
        GfVec3d realUp = GfCross(ndir, right);
        viewMatrix.SetColumn(0, GfVec4d(right[0], right[1], right[2], -GfDot(right, pos)));
        viewMatrix.SetColumn(1, GfVec4d(realUp[0], realUp[1], realUp[2], -GfDot(realUp, pos)));
        viewMatrix.SetColumn(2, GfVec4d(ndir[0], ndir[1], ndir[2], -GfDot(ndir, pos)));
        viewMatrix.SetColumn(3, GfVec4d(0,0,0,1));

        // Proj matrix
        double fov = (useDmapAutoFocus && GetNode().hasFn(MFn::kAreaLight)) ? 110.0 : dmapWidthFocus;
        fov = fov > 160.0 ? 160.0 : fov;
        auto frustumDepth = farClip - nearClip;
        frustumDepth = frustumDepth < 1.0 ? 1.0 : frustumDepth;
        nearClip = nearClip < 1.0f ? 1.0f : nearClip;

        projMatrix.SetIdentity();
        projMatrix[0][0] = 1.0 / tan(0.5 * fov * (2.0 * M_PI / 360.0) + 0.00001);
        projMatrix[1][1] = 1.0 * projMatrix[0][0];
        projMatrix[2][2] = farClip / frustumDepth;
        projMatrix[3][2] = -(farClip * nearClip) / frustumDepth;
        projMatrix[2][3] = 1.0f;
        projMatrix[3][3] = 0.0f;
    }

    // To make sure the adjusted view matrix takes effect, multiply by the light transform to counteract 
    // the ViewMaxtrix (inverted LightTransform) calculated by Hydra.
    return xform * viewMatrix * projMatrix;
}

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
        auto     intensity   = mayaLight.intensity();
#if defined(HD_API_VERSION) && HD_API_VERSION >= 74 // For USD 24.11+
        if( LightType() == HdPrimTypeTokens->simpleLight){
            intensity /= M_PI;
        }
#endif
        const bool     shadowsEnabled = GetShadowsEnabled(mayaLight);

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
        light.SetHasShadow(shadowsEnabled);
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
        // Exclude prims that should not be lighted by only taking lighted paths
        SdfPathVector lightedPaths;
        GetMayaHydraSceneIndex()->GetLightedPrimPaths(lightedPaths);
        HdRprimCollection coll(
            HdTokens->geometry,
            HdReprSelector(HdReprTokens->refined));
        coll.SetRootPaths(lightedPaths);
        return VtValue(coll);
    } else if (key == HdLightTokens->shadowParams) {
        HdxShadowParams shadowParams;
        MFnLight        mayaLight(GetDagPath());
        if (!GetShadowsEnabled(mayaLight)) {
            shadowParams.enabled = false;
        } else {
            _CalculateShadowParams(mayaLight, shadowParams);
        }
        return VtValue(shadowParams);
    }
    // Let base class handle other keys
    return MayaHydraDagAdapter::Get(key);
}

MayaHydraLightAdapter::MayaLightParams MayaHydraLightAdapter::GetMayaLightParams() const
{
    MayaLightParams params;
    MStatus status;
    MFnDependencyNode lightDepNode(GetNode(), &status);
    
    if (status == MS::kSuccess) {
        // Get intensity
        MPlug intensityPlug = lightDepNode.findPlug("intensity", true, &status);
        if (status == MS::kSuccess && !intensityPlug.isNull()) {
            params.intensity = intensityPlug.asFloat();
        }
        
        // Get color
        MPlug colorPlug = lightDepNode.findPlug("color", true, &status);
        if (status == MS::kSuccess && !colorPlug.isNull()) {
            float r = 0.5f, g = 0.5f, b = 0.5f;
            colorPlug.child(0).getValue(r);
            colorPlug.child(1).getValue(g);
            colorPlug.child(2).getValue(b);
            
            params.color = GfVec3f(r, g, b);
        }
        
        // Get shadowColor
        MPlug shadowColorPlug = lightDepNode.findPlug("shadowColor", true, &status);
        if (status == MS::kSuccess && !shadowColorPlug.isNull()) {
            float r = 0.0f, g = 0.0f, b = 0.0f;
            shadowColorPlug.child(0).getValue(r);
            shadowColorPlug.child(1).getValue(g);
            shadowColorPlug.child(2).getValue(b);
            
            params.shadowColor = GfVec3f(r, g, b);
        }
        
        // Get exposure
        MPlug exposurePlug = lightDepNode.findPlug("aiExposure", true, &status);
        if (status == MS::kSuccess && !exposurePlug.isNull()) {
            params.exposure = exposurePlug.asFloat();
        }
        
        // Get normalize
        MPlug normalizePlug = lightDepNode.findPlug("aiNormalize", true, &status);
        if (status == MS::kSuccess && !normalizePlug.isNull()) {
            params.normalize = normalizePlug.asBool();
        }
        
        // Get diffuse
        MPlug diffusePlug = lightDepNode.findPlug("aiDiffuse", true, &status);
        if (status == MS::kSuccess && !diffusePlug.isNull()) {
            params.diffuse = diffusePlug.asFloat();
        }
        
        // Get specular
        MPlug specularPlug = lightDepNode.findPlug("aiSpecular", true, &status);
        if (status == MS::kSuccess && !specularPlug.isNull()) {
            params.specular = specularPlug.asFloat();
        }
        
        // Get enableColorTemperature
        MPlug enableColorTempPlug = lightDepNode.findPlug("aiEnableTemperature", true, &status);
        if (status == MS::kSuccess && !enableColorTempPlug.isNull()) {
            params.enableColorTemperature = enableColorTempPlug.asBool();
        }
        
        // Get colorTemperature
        MPlug colorTempPlug = lightDepNode.findPlug("aiColorTemperature", true, &status);
        if (status == MS::kSuccess && !colorTempPlug.isNull()) {
            params.colorTemperature = colorTempPlug.asFloat();
        }
    }
    
    return params;
}

VtValue MayaHydraLightAdapter::GetLightParamValue(const TfToken& paramName)
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET_LIGHT_PARAM_VALUE)
        .Msg(
            "Called MayaHydraLightAdapter::GetLightParamValue(%s) - %s\n",
            paramName.GetText(),
            GetDagPath().partialPathName().asChar());

    MFnLight light(GetDagPath());
    
    // Get Maya parameters (including Arnold attributes with "ai" prefix)
    const MayaLightParams mayaParams = GetMayaLightParams();
    
    if ((paramName == HdLightTokens->color) 
        || (paramName == HdTokens->displayColor)
        || (paramName == UsdLuxTokens->inputsColor)) {
        return VtValue(mayaParams.color);
    } else if ((paramName == HdLightTokens->intensity) 
            || (paramName == UsdLuxTokens->inputsIntensity)) {
        auto intensity = mayaParams.intensity;
#if defined(HD_API_VERSION) && HD_API_VERSION >= 74 // For USD 24.11+
        if( LightType() == HdPrimTypeTokens->simpleLight){
            intensity /= M_PI;
        }
#endif
        return VtValue(intensity);
    } else if ((paramName == HdLightTokens->exposure) 
            || (paramName == UsdLuxTokens->inputsExposure)) {
        return VtValue(mayaParams.exposure);
    } else if ((paramName == HdLightTokens->normalize)
            || (paramName == UsdLuxTokens->inputsNormalize)) {
        return VtValue(mayaParams.normalize);
    } else if ((paramName == HdLightTokens->enableColorTemperature)
            || (paramName == UsdLuxTokens->inputsEnableColorTemperature)) {
        return VtValue(mayaParams.enableColorTemperature);
    } else if ((paramName == HdLightTokens->diffuse)
            || (paramName == UsdLuxTokens->inputsDiffuse)) {
        return VtValue(mayaParams.diffuse);
    } else if ((paramName == HdLightTokens->specular) 
            || (paramName == UsdLuxTokens->inputsSpecular)) {
        return VtValue(mayaParams.specular);
    } else if ((paramName == HdLightTokens->colorTemperature)
            || (paramName == UsdLuxTokens->inputsColorTemperature)) {
        return VtValue(mayaParams.colorTemperature);
    } else if ((paramName == HdLightTokens->shadowColor)
            || (paramName == UsdLuxTokens->inputsShadowColor)) {
        return VtValue(mayaParams.shadowColor);
    } else if (
            (paramName == HdLightTokens->shadowEnable) 
        ||  (paramName == HdLightTokens->hasShadow)
        ||  (paramName == UsdLuxTokens->inputsShadowEnable)
        ) {
        const bool shadowsEnabled = GetShadowsEnabled(light);
        return VtValue(shadowsEnabled);
    }
    return {};
}

// Is for PRMan and potentially other renderers that use material networks for lights.
VtValue MayaHydraLightAdapter::GetLightMaterialNetwork()const
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("Called MayaHydraLightAdapter::GetLightMaterialNetwork() - %s\n",
            GetDagPath().partialPathName().asChar());
    
    // Additional debugging for dome lights
    const bool isSkyDomeLight = IsDagPathAnArnoldSkyDomeLight(GetDagPath());
    if (isSkyDomeLight) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
            .Msg("Processing Arnold Sky Dome Light: %s\n", 
                 GetDagPath().partialPathName().asChar());
    }

    // Create material network for RenderMan lights
    HdMaterialNetworkMap networkMap;
    HdMaterialNetwork lightNetwork;
    HdMaterialNode lightNode;
    
    // Set the light node path
    lightNode.path = GetID();
    
    // Determine the appropriate PRMan light shader based on Maya light type
    MFnLight mayaLight(GetDagPath());
    MString lightType = mayaLight.typeName();
    const bool isAnArnoldAreaLight = IsDagPathAnArnoldAreaLight(GetDagPath());
    
    // Debug: Print Maya light parameters
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("Maya light parameters:\n");
    
    const auto inclusiveMatrix = GetDagPath().inclusiveMatrix();
    const bool shadowsEnabled = mayaLight.useRayTraceShadows();
    
     // Get Maya parameters (including Arnold attributes with "ai" prefix)
    MayaLightParams mayaParams = GetMayaLightParams();   
       
    lightNode.parameters[HdLightTokens->color]                  = VtValue(mayaParams.color);
    lightNode.parameters[HdLightTokens->intensity]              = VtValue(mayaParams.intensity);
    lightNode.parameters[HdLightTokens->exposure]               = VtValue(mayaParams.exposure);
    lightNode.parameters[HdLightTokens->normalize]              = VtValue(mayaParams.normalize);
    lightNode.parameters[HdLightTokens->diffuse]                = VtValue(mayaParams.diffuse);
    lightNode.parameters[HdLightTokens->specular]               = VtValue(mayaParams.specular);
    lightNode.parameters[HdLightTokens->enableColorTemperature] = VtValue(mayaParams.enableColorTemperature);
    lightNode.parameters[HdLightTokens->colorTemperature]       = VtValue(mayaParams.colorTemperature);
    lightNode.parameters[HdLightTokens->shadowEnable]           = VtValue(shadowsEnabled);
    lightNode.parameters[HdLightTokens->shadowColor]            = VtValue(mayaParams.shadowColor);
    lightNode.parameters[HdTokens->transform] = VtValue(GetGfMatrixFromMaya(inclusiveMatrix));

    // Add additional RenderMan specific parameters
    lightNode.parameters[TfToken("visibility:camera")]= VtValue(false); // true means the light shape is visible in the rendering
    
    // Default to distant light for unknown types
    lightNode.identifier = TfToken("PxrDistantLight");

    if (lightType == "directionalLight") {
        lightNode.identifier = TfToken("PxrDistantLight");
        // Directional light specific parameters
        // The following values come from OpenUSD/pxr/imaging/hdx/taskController.cpp
        // Distant Light values
        constexpr float DISTANT_LIGHT_ANGLE = 0.53f;
        lightNode.parameters[HdLightTokens->angle]      = VtValue(DISTANT_LIGHT_ANGLE);//The actual angle comes from the transform

        constexpr float DISTANT_LIGHT_INTENSITY = 15000.0f;
        const float distantLightIntensity = mayaParams.intensity * DISTANT_LIGHT_INTENSITY;
        lightNode.parameters[HdLightTokens->intensity] = distantLightIntensity;

        // The following values come from OpenUSD/pxr/imaging/hdx/taskController.cpp
        // We assume that the color specified for these "simple" lights means
        // that it is the expected color a white Lambertian surface would have
        // if one of these colored "simple" lights was pointed directly at it.
        // To achieve this, the light color needs to be scaled appropriately.
        lightNode.parameters[HdLightTokens->diffuse]    = mayaParams.diffuse  * float(M_PI);
        lightNode.parameters[HdLightTokens->specular]   = mayaParams.specular * float(M_PI);
    } else if (lightType == "pointLight") {
        lightNode.identifier = TfToken("PxrSphereLight");
        
        // Override intensity to match Storm and Arnold
        const float pointLightIntensity = mayaParams.intensity * 2.0f * M_PI;
        lightNode.parameters[HdLightTokens->intensity] = VtValue(pointLightIntensity);
        
        // PxrSphereLight is a sphere-shaped light that needs a radius
        constexpr float radius = 0.01f; // Default radius for point lights
        lightNode.parameters[HdLightTokens->radius] = VtValue(radius);

    } else if (lightType == "spotLight") {
        lightNode.identifier = TfToken("PxrDiskLight");
        
        // Override intensity to match Storm and Arnold
        const float spotLightIntensity = mayaParams.intensity * 2.0f * M_PI;
        lightNode.parameters[HdLightTokens->intensity] = VtValue(spotLightIntensity);
        
        // PxrDiskLight is a disk-shaped light that needs a radius
        // Calculate radius based on cone angle and a reasonable distance (same as in maya)
        MFnSpotLight spotLight(GetDagPath());
        const double coneAngleRadians = spotLight.coneAngle();
        constexpr float FRUSTUM_LOCATION(1.3f); // same as in maya but as a positive value, it is negative in maya
        const float     radius = static_cast<float>(tan(coneAngleRadians / 2.0) * FRUSTUM_LOCATION);
        lightNode.parameters[HdLightTokens->radius] = VtValue(radius);
        
        // Add spot light specific parameters using proper USD tokens
        lightNode.parameters[HdLightTokens->shapingConeAngle] = VtValue(spotLight.coneAngle() * 180.0 / M_PI);
        lightNode.parameters[HdLightTokens->shapingConeSoftness] = VtValue(spotLight.penumbraAngle() * 180.0 / M_PI);
    } else if ((lightType == "areaLight") || isAnArnoldAreaLight) {
        lightNode.identifier = TfToken("PxrRectLight");
        
        // Area light specific parameters using proper USD tokens
        MFnAreaLight areaLight(GetDagPath());
        constexpr float             defaultWidthForAreaLights   = 2.0f;
        constexpr float             defaultHeightForAreaLights  = 2.0f;
        double                      scale[3] = { 1.0, 1.0, 1.0 };
        const MTransformationMatrix modelMatrix(inclusiveMatrix);
        modelMatrix.getScale(scale, MSpace::kWorld);
        const float widthScaled     = defaultWidthForAreaLights * scale[0];
        const float heightScaled    = defaultHeightForAreaLights * scale[1];
        lightNode.parameters[HdLightTokens->width]  = VtValue(widthScaled);
        lightNode.parameters[HdLightTokens->height] = VtValue(heightScaled);
    } else {
        const bool isSkyDomeLight = IsDagPathAnArnoldSkyDomeLight(GetDagPath());
        if (isSkyDomeLight) {
            lightNode.identifier = TfToken("PxrDomeLight");
            
            // For the domelight, add the domelight texture resource.
            MStatus status;
            MFnDependencyNode lightDepNode(GetNode(), &status);
            const std::string domeLightTexturePath = GetDomeLightTexture(lightDepNode);
            
            if (!domeLightTexturePath.empty()) {
                // Set texture file with proper SdfAssetPath
                lightNode.parameters[HdLightTokens->textureFile] = 
                    VtValue(SdfAssetPath(domeLightTexturePath, domeLightTexturePath));
                
                // Set texture format - Get Arnold format and map to USD tokens using correct UsdLuxTokens
                MPlug formatPlug = lightDepNode.findPlug("format", true, &status);
                if (status == MS::kSuccess) {
                    const auto format = formatPlug.asShort();
                    // mirrored_ball : 0, angular : 1, latlong : 2
                    if (format == 0) {
                        lightNode.parameters[HdLightTokens->textureFormat] = VtValue(UsdLuxTokens->mirroredBall);
                    } else if (format == 2) {
                        lightNode.parameters[HdLightTokens->textureFormat] = VtValue(UsdLuxTokens->latlong);
                    } else {
                        lightNode.parameters[HdLightTokens->textureFormat] = VtValue(UsdLuxTokens->automatic);
                    }
                } else {
                    // Default to automatic if format plug not found
                    lightNode.parameters[HdLightTokens->textureFormat]
                        = VtValue(UsdLuxTokens->automatic);
                }
                
                // When texture is connected, use white color (matching aiSkyDomeLightAdapter)
                lightNode.parameters[HdLightTokens->color] = VtValue(GfVec3f(1.0f, 1.0f, 1.0f));
                // Override intensity to match Storm and Arnold
                const float domeLightIntensity = mayaParams.intensity * M_PI;
                lightNode.parameters[HdLightTokens->intensity] = domeLightIntensity;
                
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                    .Msg("Dome light texture path: %s\n", domeLightTexturePath.c_str());
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                    .Msg("Using white color (1,1,1) for dome light with texture\n");
            } else {
                // Handle case where no texture is connected
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                    .Msg("Warning: No texture found for dome light %s - using color only\n", 
                         GetDagPath().partialPathName().asChar());
                
                // For dome lights without texture, don't set textureFile parameter
                // PRMan should handle this case gracefully
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                    .Msg("Using Arnold color and fallback intensity for dome light without texture\n");
            }
            
            // Debug: Print all dome light parameters with values (AFTER all parameters are set)
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("Dome light final parameters:\n");
            for (const auto& param : lightNode.parameters) {
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                    .Msg("  %s: %s\n", param.first.GetText(), param.second.GetTypeName().c_str());
            }
            
            // Debug: Print specific key parameter values (AFTER all parameters are set)
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("Dome light key values:\n");
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  intensity: %f\n", lightNode.parameters[HdLightTokens->intensity].Get<float>());
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  exposure: %f\n", lightNode.parameters[HdLightTokens->exposure].Get<float>());
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  normalize: %s\n", lightNode.parameters[HdLightTokens->normalize].Get<bool>() ? "true" : "false");
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  shadowEnable: %s\n", lightNode.parameters[HdLightTokens->shadowEnable].Get<bool>() ? "true" : "false");
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  cameraVisibility: %s\n", lightNode.parameters[TfToken("cameraVisibility")].Get<bool>() ? "true" : "false");
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  primaryVisibility: %s\n", lightNode.parameters[TfToken("primaryVisibility")].Get<bool>() ? "true" : "false");
            
            // Debug: Print final color value
            GfVec3f finalColor = lightNode.parameters[HdLightTokens->color].Get<GfVec3f>();
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
                .Msg("  final color: (%f, %f, %f)\n", finalColor[0], finalColor[1], finalColor[2]);
        }
    }
    
    // Add the light node to the network
    lightNetwork.nodes.push_back(lightNode);
    
    // Add the network to the material network map with 'light' terminal
    networkMap.map[HdMaterialTerminalTokens->light] = lightNetwork;
    networkMap.terminals.push_back(lightNode.path);
    
    // Debug: Print final material network structure
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("Final material network for light %s:\n", GetDagPath().partialPathName().asChar());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Light type: %s\n", lightType.asChar());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Light identifier: %s\n", lightNode.identifier.GetText());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Light path: %s\n", lightNode.path.GetText());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Terminal: %s\n", lightNode.path.GetText());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Number of parameters: %zu\n", lightNode.parameters.size());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Network map size: %zu\n", networkMap.map.size());
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Terminals size: %zu\n", networkMap.terminals.size());
    
    // Debug: Verify the material network is properly structured
    if (networkMap.map.find(HdMaterialTerminalTokens->light) != networkMap.map.end()) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
            .Msg("  Material network has 'light' terminal: YES\n");
    } else {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
            .Msg("  Material network has 'light' terminal: NO - ERROR!\n");
    }
    
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("  Returning material network for light: %s\n", GetDagPath().partialPathName().asChar());
    
    return VtValue(networkMap);
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

    params.shadowMatrix = std::make_shared<MayaHydraConstantShadowMatrix>(_CalculateShadowProjectionMatrix());

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

PXR_NAMESPACE_CLOSE_SCOPE
