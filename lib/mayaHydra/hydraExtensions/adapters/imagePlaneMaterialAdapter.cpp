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
// Material adapter for Maya image plane nodes.
//
// Maya image planes carry their texture via the "imageName" attribute rather
// than through a shading-engine / surface-shader connection like regular
// geometry.  This adapter builds a Hydra material network
// (UsdPreviewSurface + UsdUVTexture + UsdPrimvarReader_float2) that reads
// the image directly from the image plane node.
//
// Filename resolution (including animated image sequences) is delegated to
// MRenderUtil::exactImagePlaneFileName so that behaviour matches Maya's own
// image plane evaluation.  Movie/video files are detected and reported with
// a warning since Hydra does not support them.
//

#include "imagePlaneMaterialAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <maya/MEventMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

#include <string>

using namespace MayaHydra;

PXR_NAMESPACE_OPEN_SCOPE

namespace {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (imagePlaneSurface)
    (imagePlaneTexture)
    (imagePlanePrimvar)
    // Signals PruneTexturesSceneIndex to keep textures visible even when
    // the viewport "Textured" mode is off, matching VP2 image plane behaviour.
    (_alwaysShowTextures)
);

void _AttributeChangedCallback(
    MNodeMessage::AttributeMessage msg,
    MPlug&                         plug,
    MPlug&                         otherPlug,
    void*                          clientData)
{
    TF_UNUSED(msg);
    TF_UNUSED(otherPlug);
    auto* adapter
        = reinterpret_cast<MayaHydraImagePlaneMaterialAdapter*>(clientData);

    if (MayaHydraAdapter::IsExtensionOrDynamicAttribute(plug)) {
        adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
        return;
    }

    MObject attr = plug.attribute();
    if (attr == MayaAttrs::imagePlane::imageName
        || attr == MayaAttrs::imagePlane::useFrameExtension
        || attr == MayaAttrs::imagePlane::frameExtension) {
        adapter->MarkDirty(HdMaterial::AllDirty);
    }
}

// Maya's addAttributeChangedCallback does not reliably fire for
// expression-driven attributes (such as frameExtension connected to
// the time node) during playback.  This event callback ensures the
// material is re-evaluated whenever the current frame changes.
void _TimeChangedCallback(void* clientData)
{
    auto* adapter
        = reinterpret_cast<MayaHydraImagePlaneMaterialAdapter*>(clientData);

    MStatus status;
    MPlug useFrameExtPlug(
        adapter->GetNode(), MayaAttrs::imagePlane::useFrameExtension);
    if (useFrameExtPlug.asBool(&status) && status) {
        adapter->MarkDirty(HdMaterial::AllDirty);
    }
}

} // anonymous namespace

MayaHydraImagePlaneMaterialAdapter::MayaHydraImagePlaneMaterialAdapter(
    const SdfPath&        id,
    MayaHydraSceneIndex*  mayaHydraSceneIndex,
    const MObject&        obj)
    : MayaHydraMaterialAdapter(id, mayaHydraSceneIndex, obj)
{
}

void MayaHydraImagePlaneMaterialAdapter::CreateCallbacks()
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Creating image plane material adapter callbacks for prim (%s).\n",
             GetID().GetText());

    MStatus status;
    auto obj = GetNode();
    auto id = MNodeMessage::addAttributeChangedCallback(
        obj, _AttributeChangedCallback, this, &status);
    if (status) {
        AddCallback(id);
    }

    auto timeId = MEventMessage::addEventCallback(
        "timeChanged", _TimeChangedCallback, this, &status);
    if (status) {
        AddCallback(timeId);
    }

    MayaHydraAdapter::CreateCallbacks();
}

VtValue MayaHydraImagePlaneMaterialAdapter::GetMaterialResource()
{
    std::string imagePath = GetImagePlaneTexturePath(GetNode());
    if (imagePath.empty()) {
        return GetPreviewMaterialResource(GetID());
    }

    HdMaterialNetworkMap networkMap;
    HdMaterialNetwork network;

    SdfPath surfacePath = GetID().AppendChild(_tokens->imagePlaneSurface);
    SdfPath texturePath = GetID().AppendChild(_tokens->imagePlaneTexture);
    SdfPath primvarPath = GetID().AppendChild(_tokens->imagePlanePrimvar);

    // UsdPrimvarReader_float2 node
    HdMaterialNode primvarNode;
    primvarNode.path = primvarPath;
    primvarNode.identifier = UsdImagingTokens->UsdPrimvarReader_float2;
    primvarNode.parameters[MayaHydraAdapterTokens->varname]
        = MayaHydraAdapterTokens->st;
    network.nodes.push_back(primvarNode);

    // UsdUVTexture node
    HdMaterialNode textureNode;
    textureNode.path = texturePath;
    textureNode.identifier = UsdImagingTokens->UsdUVTexture;
    textureNode.parameters[MayaHydraAdapterTokens->file]
        = SdfAssetPath(imagePath, imagePath);
    network.nodes.push_back(textureNode);

    // UsdPreviewSurface configured as unlit: image planes display a flat
    // texture without lighting interaction, matching VP2 behaviour.
    // The texture drives emissiveColor while diffuseColor is black and
    // roughness is 1.0 to eliminate specular highlights.
    HdMaterialNode surfaceNode;
    surfaceNode.path = surfacePath;
    surfaceNode.identifier = UsdImagingTokens->UsdPreviewSurface;
    for (const auto& it : MayaHydraMaterialNetworkConverter::GetPreviewShaderParams()) {
        surfaceNode.parameters.emplace(it.name, it.fallbackValue);
    }
    // Zero out diffuse so scene lights don't add shading on top of the image.
    surfaceNode.parameters[MayaHydraAdapterTokens->diffuseColor]
        = VtValue(GfVec3f(0.0f, 0.0f, 0.0f));
    // Max roughness eliminates specular highlights that would otherwise
    // alter the image colours (default roughness 0.5 + ior 1.5 creates
    // visible reflections that lighten dark areas of the texture).
    surfaceNode.parameters[MayaHydraAdapterTokens->roughness]
        = VtValue(1.0f);
    surfaceNode.parameters[_tokens->_alwaysShowTextures] = VtValue(true);
    network.nodes.push_back(surfaceNode);

    // primvarReader:result -> texture:st
    {
        HdMaterialRelationship rel;
        rel.inputId = primvarPath;
        rel.inputName = MayaHydraAdapterTokens->result;
        rel.outputId = texturePath;
        rel.outputName = MayaHydraAdapterTokens->st;
        network.relationships.push_back(rel);
    }

    // texture:rgb -> surface:emissiveColor (unlit flat image)
    {
        HdMaterialRelationship rel;
        rel.inputId = texturePath;
        rel.inputName = MayaHydraAdapterTokens->rgb;
        rel.outputId = surfacePath;
        rel.outputName = MayaHydraAdapterTokens->emissiveColor;
        network.relationships.push_back(rel);
    }

    networkMap.map[HdMaterialTerminalTokens->surface] = network;
    networkMap.terminals.push_back(surfacePath);

    return VtValue(networkMap);
}

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraImagePlaneMaterialAdapter, TfType::Bases<MayaHydraMaterialAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, imagePlane)
{
    MayaHydraAdapterRegistry::RegisterMaterialAdapter(
        TfToken("imagePlane"),
        [](const SdfPath&        id,
            MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MObject&        obj) -> MayaHydraMaterialAdapterPtr {
            return MayaHydraMaterialAdapterPtr(
                new MayaHydraImagePlaneMaterialAdapter(id, mayaHydraSceneIndex, obj));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
