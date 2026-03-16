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
#ifndef MAYAHYDRALIB_LIGHT_ADAPTER_H
#define MAYAHYDRALIB_LIGHT_ADAPTER_H

#include <mayaHydraLib/adapters/dagAdapter.h>

#include <pxr/base/gf/frustum.h>
#include <pxr/imaging/glf/simpleLight.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hdx/simpleLightTask.h>
#include <pxr/pxr.h>

#include <maya/MFnLight.h>
#include <maya/MFnNonExtendedLight.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

#include <string>
#include <unordered_set>

PXR_NAMESPACE_OPEN_SCOPE

/// Callback for parent node attribute changes. Shared by MayaHydraLightAdapter and subclasses
/// (e.g. MayaHydraAiAreaLightAdapter). Use with MNodeMessage::addAttributeChangedCallback.
void LightAdapterParentAttributeChanged(
    MNodeMessage::AttributeMessage msg,
    MPlug&                         plug,
    MPlug&                         otherPlug,
    void*                          clientData);

class MayaHydraSceneIndex;

/**
 * \brief MayaHydraLightAdapter is the base class for any light adapter used to handle the
 * translation from a light to hydra.
 */
class MayaHydraLightAdapter : public MayaHydraDagAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag);
    MAYAHYDRALIB_API
    virtual ~MayaHydraLightAdapter();
    MAYAHYDRALIB_API
    virtual const TfToken& LightType() const = 0;
    MAYAHYDRALIB_API
    bool IsSupported() const override;
    MAYAHYDRALIB_API
    void Populate() override;
    MAYAHYDRALIB_API
    void MarkDirty(HdDirtyBits dirtyBits) override;
    MAYAHYDRALIB_API
    virtual void RemovePrim() override;
    MAYAHYDRALIB_API
    bool HasType(const TfToken& typeId) const override;
    MAYAHYDRALIB_API
    virtual VtValue GetLightParamValue(const TfToken& paramName);
    MAYAHYDRALIB_API
    VtValue Get(const TfToken& key) override;
    MAYAHYDRALIB_API
    virtual VtValue GetLightMaterialNetwork() const; // Is for PRMan
    MAYAHYDRALIB_API
    virtual void CreateCallbacks() override;

    // Helper struct and method for Maya light parameters
    struct MayaLightParams
    {
        float   intensity = 1.0f;
        GfVec3f color { 1.0f, 1.0f, 1.0f };
        GfVec3f shadowColor { 0.0f, 0.0f, 0.0f };
        float   exposure = 0.0f;
        bool    normalize = true;
        float   diffuse = 1.0f;
        float   specular = 1.0f;
        bool    enableColorTemperature = false;
        float   colorTemperature = 6500.0f;
    };

    MAYAHYDRALIB_API
    MayaLightParams GetMayaLightParams() const;
    MAYAHYDRALIB_API
    TfToken GetRenderTag() const override;

    /// Lights opt in so extension/dynamic attrs are included in primvars.
    /// Built-in light attrs are still filtered by GetAttributesFromNode.
    MAYAHYDRALIB_API
    bool IncludeAllAttributesInPrimvars() const override { return true; }

    bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const override;
    HdDirtyBits GetExtraDirtyBitsForPrimvarAttributeChange(const MPlug& plug) const override;

    bool GetShadowsEnabled(MFnLight& light) const;

    void GetGlfSimpleLightPosAndDirFromMFnLight(MFnLight& light, GlfSimpleLight& outSimpleLight);

    /// For plug dirty callbacks: marks DirtyParams/DirtyShadowParams only when the plug affects
    /// light params. Primvar-only attrs (e.g. aiShadowDensity) skip this to avoid over-dirtying.
    static void MarkDirtyIfPlugAffectsLightParams(MayaHydraLightAdapter* adapter, const MPlug& plug);

    /// For unit tests: returns the param attribute names that trigger DirtyParams.
    /// Use with LightPrimvars.ParamAttributesMatchGetLogic to ensure the list stays in sync.
    MAYAHYDRALIB_API
    static const std::unordered_set<std::string>& GetLightParamAttributeNamesForTest();

protected:
    /// Override to handle shape attribute changes before the default logic. Return true if fully
    /// handled (e.g. ai_translator on aiAreaLight). Default returns false.
    virtual bool OnShapeAttributeChanged(const MPlug& plug);

    /// Static callback for MNodeMessage::addAttributeChangedCallback on the shape node.
    static void _LightShapeAttributeChanged(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData);

    MAYAHYDRALIB_API
    virtual void _CalculateLightParams(GlfSimpleLight& light) { }
    MAYAHYDRALIB_API
    void _CalculateShadowParams(MFnLight& light, HdxShadowParams& params);
    MAYAHYDRALIB_API
    bool _GetVisibility() const override;

    GfMatrix4d _CalculateShadowProjectionMatrix();

    bool _isLightingOn = true;
};

using MayaHydraLightAdapterPtr = std::shared_ptr<MayaHydraLightAdapter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_LIGHT_ADAPTER_H