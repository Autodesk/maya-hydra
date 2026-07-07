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
    /// Create a light adapter for the given DAG path.
    MayaHydraLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag);
    MAYAHYDRALIB_API
    /// Destroy the light adapter and remove callbacks.
    virtual ~MayaHydraLightAdapter();
    MAYAHYDRALIB_API
    /// Return the Hydra light type token for this adapter.
    virtual const TfToken& LightType() const = 0;
    MAYAHYDRALIB_API
    /// Return whether this light type is supported by the render index.
    bool IsSupported() const override;
    MAYAHYDRALIB_API
    /// Insert the light prim into the scene index.
    void Populate() override;
    MAYAHYDRALIB_API
    /// Remove the light prim from the scene index.
    virtual void RemovePrim() override;
    MAYAHYDRALIB_API
    /// Return whether this adapter matches the given Hydra type id.
    bool HasType(const TfToken& typeId) const override;
    MAYAHYDRALIB_API
    /// Return a specific Hydra light parameter value.
    virtual VtValue GetLightParamValue(const TfToken& paramName);
    MAYAHYDRALIB_API
    /// Return a value for the requested Hydra data source key.
    VtValue Get(const TfToken& key) override;
    MAYAHYDRALIB_API
    /// Return a light material network (used by PRMan).
    virtual VtValue GetLightMaterialNetwork() const; // Is for PRMan
    MAYAHYDRALIB_API
    /// Register Maya callbacks for light changes.
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
    /// Return the current Maya light parameter bundle.
    MayaLightParams GetMayaLightParams() const;
    MAYAHYDRALIB_API
    /// Return the Hydra render tag for this light.
    TfToken GetRenderTag() const override;

    /// Suppress primvar dirtying for built-in light params that already dirty schema bits.
    bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const override;
    /// Add light schema + visibility + collections locators on top of the base primvar
    /// invalidation when an extension/dynamic light-param attribute changes, matching the
    /// full SprimDirtyBitsToLocatorSet(DirtyParams) expansion used for built-in params.
    void AddExtraDirtyForPrimvarAttributeChange(Fvp::FvpDirtyNotifier& notifier, const MPlug& plug) override;

    /// Return whether shadows are enabled for this light.
    bool GetShadowsEnabled(MFnLight& light) const;

    /// Compute GlfSimpleLight position/direction from a Maya light.
    void GetGlfSimpleLightPosAndDirFromMFnLight(MFnLight& light, GlfSimpleLight& outSimpleLight);

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
    /// Populate the GlfSimpleLight params from the Maya light.
    virtual void _CalculateLightParams(GlfSimpleLight& light) { }
    MAYAHYDRALIB_API
    /// Populate HdxShadowParams from the Maya light.
    void _CalculateShadowParams(MFnLight& light, HdxShadowParams& params);
    MAYAHYDRALIB_API
    /// Return the visibility state for this light.
    bool _GetVisibility() const override;

    /// Compute the shadow projection matrix for this light.
    GfMatrix4d _CalculateShadowProjectionMatrix();

    bool _isLightingOn = true;
};

using MayaHydraLightAdapterPtr = std::shared_ptr<MayaHydraLightAdapter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_LIGHT_ADAPTER_H