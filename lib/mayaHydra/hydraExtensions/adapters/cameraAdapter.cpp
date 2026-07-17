//
// Copyright 2021 Autodesk
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
#include "cameraAdapter.h"

#include <mayaHydraLib/adapters/mhDirtyNotifier.h>

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/gf/interval.h>
#include <pxr/imaging/hd/camera.h>
#include <pxr/imaging/hd/changeTracker.h>

#include <maya/MDagMessage.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnCamera.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace {

// Attributes that affect HdCamera params (from GetCameraParamValue). When these change,
// we mark DirtyParams | DirtyPrimvar in one call to avoid duplicates.
static const char* const kCameraParamAttributeNames[] = {
    "nearClipPlane", "farClipPlane", "shutterAngle", "focusDistance", "focalLength",
    "fStop", "horizontalFilmAperture", "verticalFilmAperture", "lensSqueezeRatio",
    "shakeEnabled", "horizontalFilmOffset", "horizontalShake", "verticalFilmOffset",
    "verticalShake", "filmFit", "depthOfField", "orthographic",
};

static void _cameraPlugDirty(MObject& node, MPlug& plug, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraCameraAdapter*>(clientData);
    if (plug.isChild()) {
        return;
    }
    MPlug topPlug = MayaHydra::GetTopPlug(plug);
    // Only handle driven plugs here; direct setAttr changes are handled in the
    // attribute-changed callback to avoid duplicate dirty notifications.
    if (!topPlug.isConnected()) {
        return;
    }
    if (MayaHydraAdapter::IsParamAttribute(
            topPlug,
            MayaHydraAdapter::GetParamAttributeSet(kCameraParamAttributeNames))) {
        MayaHydra::DirtyNotifier(adapter).dirtyCameraParams().dirtyPrimvars();
    } else {
        adapter->MaybeMarkPrimvarDirtyForAttributeChange(topPlug);
    }
}

// Fires on attribute value changes and DG structure changes (add/remove/rename). Ensures
// extension attrs like aiUScale trigger primvar dirty, and undo/redo of addAttr refreshes primvars.
static void _cameraAttributeChanged(
    MNodeMessage::AttributeMessage msg,
    MPlug&                        plug,
    MPlug&                        otherPlug,
    void*                         clientData)
{
    TF_UNUSED(otherPlug);
    auto* adapter = reinterpret_cast<MayaHydraCameraAdapter*>(clientData);
    if (!MayaHydraAdapter::AttributeMessageAffectsExtensionPrimvars(msg)) {
        return;
    }
    MPlug topPlug = MayaHydra::GetTopPlug(plug);
    // Driven plug changes are handled by the node-dirty callback.
    if (topPlug.isConnected()) {
        return;
    }
    if (MayaHydraAdapter::IsParamAttribute(
            topPlug,
            MayaHydraAdapter::GetParamAttributeSet(kCameraParamAttributeNames))) {
        // dirtyPrimvars() is intentional: cameras are sprims but support extension-attribute
        // primvars (custom Maya attrs translated as constant primvars). The broad primvars
        // locator ensures those are re-pulled alongside the camera schema.
        MayaHydra::DirtyNotifier(adapter).dirtyCameraParams().dirtyPrimvars();
        return;
    }
    if (!adapter->ShouldMarkPrimvarDirtyForAttributeChange(topPlug)) {
        return;
    }
    adapter->MaybeMarkPrimvarDirtyForAttributeChange(topPlug);
}

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (depthOfField)
);

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraCameraAdapter, TfType::Bases<MayaHydraShapeAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, camera)
{
    MayaHydraAdapterRegistry::RegisterCameraAdapter(
        HdPrimTypeTokens->camera,
        [](MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag) -> MayaHydraCameraAdapterPtr {
            return MayaHydraCameraAdapterPtr(new MayaHydraCameraAdapter(mayaHydraSceneIndex, dag));
        });
}

} // namespace

// MayaHydraCameraAdapter is used to handle the translation from a Maya camera to hydra.
MayaHydraCameraAdapter::MayaHydraCameraAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
    : MayaHydraShapeAdapter(mayaHydraSceneIndex->GetPrimPath(dag, true), mayaHydraSceneIndex, dag)
{
}

MayaHydraCameraAdapter::~MayaHydraCameraAdapter() { }

TfToken MayaHydraCameraAdapter::CameraType() { return HdPrimTypeTokens->camera; }

bool MayaHydraCameraAdapter::IsSupported() const
{
    return GetMayaHydraSceneIndex()->IsSprimTypeSupported(CameraType());
}

void MayaHydraCameraAdapter::Populate()
{
    if (_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->InsertPrim(this, CameraType(), GetID());
    _isPopulated = true;
}

void MayaHydraCameraAdapter::CreateCallbacks()
{
    MStatus status;
    auto    dag = GetDagPath();
    auto    obj = dag.node();

    auto plugDirty = MNodeMessage::addNodeDirtyPlugCallback(obj, _cameraPlugDirty, this, &status);
    if (status) {
        AddCallback(plugDirty);
    }

    auto attrChanged = MNodeMessage::addAttributeChangedCallback(obj, _cameraAttributeChanged, this, &status);
    if (status) {
        AddCallback(attrChanged);
    }

    auto xformChanged = MDagMessage::addWorldMatrixModifiedCallback(
        dag,
        +[](MObject& transformNode, MDagMessage::MatrixModifiedFlags& modified, void* clientData) {
            auto* adapter = reinterpret_cast<MayaHydraCameraAdapter*>(clientData);
            adapter->InvalidateTransform();
            MayaHydra::DirtyNotifier(adapter).dirtyTransform();
        },
        reinterpret_cast<void*>(this),
        &status);
    if (status) {
        AddCallback(xformChanged);
    }

    // Skip over MayaHydraShapeAdapter's CreateCallbacks
    MayaHydraAdapter::CreateCallbacks();
}

void MayaHydraCameraAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

bool MayaHydraCameraAdapter::HasType(const TfToken& typeId) const { return typeId == CameraType(); }

VtValue MayaHydraCameraAdapter::Get(const TfToken& key) { return MayaHydraShapeAdapter::Get(key); }

VtValue MayaHydraCameraAdapter::GetCameraParamValue(const TfToken& paramName)
{
    constexpr double inchToMM = 25.4;

    MStatus status;

    auto convertFit = [&](const MFnCamera& camera) -> CameraUtilConformWindowPolicy {
        const auto mayaFit = camera.filmFit(&status);
        switch (mayaFit) {
            case MFnCamera::kFillFilmFit:
                return CameraUtilConformWindowPolicy::CameraUtilCrop;
            case MFnCamera::kHorizontalFilmFit:
                return CameraUtilConformWindowPolicy::CameraUtilMatchHorizontally;
            case MFnCamera::kVerticalFilmFit:
                return CameraUtilConformWindowPolicy::CameraUtilMatchVertically;
            default:
                return CameraUtilConformWindowPolicy::CameraUtilFit;
        }
    };

    auto hadError = [&](MStatus& status) -> bool {
        if (status)
            return false;
        TF_WARN(
            "Error in MayaHydraCameraAdapter::GetCameraParamValue(%s): %s",
            paramName.GetText(),
            status.errorString().asChar());
        return false;
    };

    MFnCamera camera(GetDagPath(), &status);
    if (hadError(status))
        return {};

    const bool isOrtho = camera.isOrtho(&status);
    if (hadError(status)) {
        return {};
    }

    if (paramName == HdCameraTokens->shutterOpen) {
        // No motion samples, instantaneous shutter
        if (!GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled())
            return VtValue(double(0));
        return VtValue(double(GetMayaHydraSceneIndex()->GetCurrentTimeSamplingInterval().GetMin()));
    }
    if (paramName == HdCameraTokens->shutterClose) {
        // No motion samples, instantaneous shutter
        if (!GetMayaHydraSceneIndex()->GetParams().motionSamplesEnabled())
            return VtValue(double(0));
        const auto shutterAngle = camera.shutterAngle(&status);
        if (hadError(status))
            return {};
        auto constexpr maxRadians = M_PI * 2.0;
        auto shutterClose = std::min(std::max(0.0, shutterAngle), maxRadians) / maxRadians;
        auto interval = GetMayaHydraSceneIndex()->GetCurrentTimeSamplingInterval();
        return VtValue(double(interval.GetMin() + interval.GetSize() * shutterClose));
    }

    // Don't bother with anything else for orthographic cameras
    if (isOrtho) {
        return {};
    }
    if (paramName == HdCameraTokens->focusDistance) {
        auto focusDistance = camera.focusDistance(&status);
        if (hadError(status))
            return {};
        return VtValue(float(focusDistance));
    }
    if (paramName == HdCameraTokens->focalLength) {
        auto focalLength = camera.focalLength(&status);
        if (hadError(status))
            return {};
        return VtValue(float(focalLength)); /// focalLength is in mm, so no conversion needed
    }
    if (paramName == HdCameraTokens->clippingRange) {
        const double cameraNear = camera.nearClippingPlane();
        const double cameraFar = camera.farClippingPlane();
        return VtValue(GfRange1f(cameraNear, cameraFar));
    }
    if (paramName == HdCameraTokens->fStop) {
        // For USD/Hydra fStop=0 should disable depthOfField
        if (!camera.isDepthOfField())
            return VtValue(0.f);
        const auto fStop = camera.fStop(&status);
        if (hadError(status))
            return {};
        return VtValue(float(fStop));
    }
    if (paramName == HdCameraTokens->horizontalAperture) {
        // Lens squeeze ratio applies horizontally only.
        const double horizontalAperture =  camera.horizontalFilmAperture(&status) * camera.lensSqueezeRatio(&status);
        if (hadError(status))
            return {};
        return VtValue(float(horizontalAperture * inchToMM));
    }
    if (paramName == HdCameraTokens->verticalAperture) {
        const double verticalAperture = camera.verticalFilmAperture();
        if (hadError(status))
            return {};
        return VtValue(float(verticalAperture * inchToMM));
    }
    if (paramName == HdCameraTokens->horizontalApertureOffset) {
        // Film offset and shake (when enabled) have the same effect on film back
        const double horizontalApertureOffset =
            (camera.shakeEnabled(&status) ? camera.horizontalFilmOffset(&status) + camera.horizontalShake(&status)
                                  : camera.horizontalFilmOffset(&status));
        if (hadError(status))
            return {};
        return VtValue(float(horizontalApertureOffset * inchToMM));
    }
    if (paramName == HdCameraTokens->verticalApertureOffset) {
        // Film offset and shake (when enabled) have the same effect on film back
        const double verticalApertureOffset =
            (camera.shakeEnabled(&status) ? camera.verticalFilmOffset(&status) + camera.verticalShake(&status)
                                  : camera.verticalFilmOffset(&status));
        if (hadError(status))
            return {};
        return VtValue(float(verticalApertureOffset * inchToMM));
    }
    if (paramName == HdCameraTokens->windowPolicy) {
        const auto windowPolicy = convertFit(camera);
        if (hadError(status))
            return {};
        return VtValue(windowPolicy);
    }
    if (paramName == HdCameraTokens->projection) {
        if (isOrtho) {
            return VtValue(HdCamera::Orthographic);
        } else {
            return VtValue(HdCamera::Perspective);
        }
    }
    if (paramName == _tokens->depthOfField) {
        const bool depthOfField = camera.isDepthOfField(&status);
        if (hadError(status))
            return {};
        return VtValue(depthOfField);
    }
    return {};
}

bool MayaHydraCameraAdapter::ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const
{
    return MayaHydraAdapter::ShouldMarkPrimvarDirtyForParamAttrs(plug, kCameraParamAttributeNames);
}

const std::unordered_set<std::string>& MayaHydraCameraAdapter::GetCameraParamAttributeNamesForTest()
{
    return MayaHydraAdapter::GetParamAttributeSet(kCameraParamAttributeNames);
}

void MayaHydraCameraAdapter::SetViewport(const GfVec4d& viewport)
{
    if (!_viewport) {
        _viewport.reset(new GfVec4d);
    }
    *_viewport = viewport;
}

PXR_NAMESPACE_CLOSE_SCOPE
