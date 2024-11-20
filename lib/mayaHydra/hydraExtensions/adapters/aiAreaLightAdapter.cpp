//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/dagAdapter.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/pxr.h>

#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

PXR_NAMESPACE_OPEN_SCOPE


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

void _lightShapeChangedCallBack(
    MNodeMessage::AttributeMessage msg,
    MPlug&                         plug,
    MPlug&                         otherPlug,
    void*                          clientData)
{
    TF_UNUSED(msg);
    TF_UNUSED(otherPlug);

    auto plugPartialName = plug.partialName();
    if (plugPartialName == MString("ai_translator")) {
        // This is changing the light type, we need to remove the prim and add it again
        auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
        adapter->RemovePrim();
        adapter->Populate();
        adapter->InvalidateTransform();
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

} // namespace

/**
 * \brief aiAreaLightAdapter is used to handle the translation from a Maya area light to
 * hydra.
 */
class aiAreaLightAdapter : public MayaHydraLightAdapter
{
public:
    aiAreaLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraLightAdapter(mayaHydraSceneIndex, dag)
    {
    }

    const TfToken& LightType() const override
    {
        const TfToken& defaultLightType = HdPrimTypeTokens->rectLight;
        
        //Get the light type
        MStatus           status;
        MFnDependencyNode depNode(GetNode(), &status);
        if (!status) {
            return defaultLightType;
        }

        MString primitiveType = "quad";
        MPlug plug = depNode.findPlug("aiTranslator", true, &status);
        if (status && !plug.isNull()) {
            primitiveType = plug.asString();
            if (primitiveType.length() == 0){
                return defaultLightType;
            }
        }

        if (primitiveType == "quad") {
            return defaultLightType;
        }else if (primitiveType == "disk") {
            return HdPrimTypeTokens->diskLight;
        }else if (primitiveType == "cylinder") {
            return HdPrimTypeTokens->cylinderLight;
        }

        return defaultLightType;
    }

    VtValue GetLightParamValue(const TfToken& paramName) override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET_LIGHT_PARAM_VALUE)
            .Msg(
                "Called aiAreaLightAdapter::GetLightParamValue(%s) - %s\n",
                paramName.GetText(),
                GetDagPath().partialPathName().asChar());

        MStatus status;
        MFnDependencyNode depNode(GetNode(), &status);
        if (! status) {
            return {};
        }
        
        // the aiAreaLight has no width/height/radius/length attributes, so we need to calculate
        // them from the scale
        MString               primitiveType = "quad";
        double                scale2[3] = { 1.0, 1.0, 1.0 };

        MPlug plug = depNode.findPlug("aiTranslator", true, &status);
        MTransformationMatrix modelMatrix(GetDagPath().inclusiveMatrix());
        if (status && !plug.isNull()) {
            primitiveType = plug.asString();
            if (primitiveType.length() == 0)
                primitiveType = "quad";
        }
        
        if (primitiveType == "quad") {
            modelMatrix.getScale(scale2, MSpace::kWorld);
        }
        if (primitiveType == "disk") {
            double scale[3];
            modelMatrix.getScale(scale, MSpace::kWorld);
            if (scale[0] != scale[1]) // non uniform scaling across x and y
            {
                if (scale[0] != 0.0)
                    scale2[0] /= scale[0];
                if (scale[1] != 0)
                    scale2[1] /= scale[1];
                const double avs = (scale[0] + scale[1]) * 0.5;
                scale2[0] *= avs;
                scale2[1] *= avs;
            }

        } else if (primitiveType == "cylinder") {
            double scale[3];
            modelMatrix.getScale(scale, MSpace::kWorld);
            if (scale[0] != scale[2]) // non uniform scaling across x and z
            {
                if (scale[0] != 0.0)
                    scale2[0] /= scale[0];
                if (scale[2] != 0)
                    scale2[2] /= scale[2];
                const double avs = (scale[0] + scale[2]) * 0.5;
                scale2[0] *= avs;
                scale2[2] *= avs;
            }
        }

        auto getFloatFromMPlug = [&depNode](const char* attrName, float initVal) {
            float val = initVal;
            MStatus status;
            MPlug plug = depNode.findPlug(attrName, true, &status);
            if (status && !plug.isNull()) {
                val = plug.asFloat();
            }
            return val;
        };

        auto getBoolFromMPlug = [&depNode](const char* attrName, bool initVal) {
            bool  val = initVal;
            MStatus status;
            MPlug   plug = depNode.findPlug(attrName, true, &status);
            if (status && !plug.isNull()) {
                val = plug.asBool();
            }
            return val;
        };
        
        constexpr float defaultWidth  = 2.0f;//By default the drawing of the light shape has a width and height of 2.0
        constexpr float defaultHeight = 2.0f;
        
        //The width, height, length, radius are only queried by Hydra if the "normalize" (aiNormalize below) attribute is unchecked
        if (paramName == HdLightTokens->width) {//Rect
            return VtValue(float(defaultWidth  * scale2[0]));
        }else if (paramName == HdLightTokens->height) { //Rect
            return VtValue(float(defaultHeight * scale2[1]));
        }else if (paramName == HdLightTokens->radius) { //Cylinder, sphere and disk
            return VtValue(float(scale2[0]));
        }else if (paramName == HdLightTokens->length) {//Cylinder
            return VtValue(float(scale2[0]));
        }else if (paramName == HdLightTokens->intensity) {
            float intensity = getFloatFromMPlug("intensity", 1.0f);
            return VtValue(intensity);
        }else if (paramName == HdLightTokens->exposure) {
            float exposure = getFloatFromMPlug("aiExposure", 0.0f);
            return VtValue(exposure);
        }else if (paramName == HdLightTokens->color || paramName == HdTokens->displayColor) {
            float colorR = getFloatFromMPlug("colorR", 1.0f);
            float colorG = getFloatFromMPlug("colorG", 1.0f);
            float colorB = getFloatFromMPlug("colorB", 1.0f);
            return VtValue(GfVec3f(colorR, colorG, colorB));
        } else if (paramName == HdLightTokens->enableColorTemperature){
            const bool enableColorTemperature = getBoolFromMPlug("aiUseColorTemperature", false);
            return VtValue(enableColorTemperature);
        } else if (paramName == HdLightTokens->colorTemperature){
            float  colorTemperature = getFloatFromMPlug("aiColorTemperature", 6500.f);
            return VtValue(colorTemperature);
        } else if (paramName == HdLightTokens->diffuse) {
            return VtValue(1.0f);
        } else if (paramName == HdLightTokens->specular) {
            return VtValue(1.0f);
        } else if (paramName == HdLightTokens->normalize){
            const bool normalize = getBoolFromMPlug("aiNormalize", false);
            return VtValue(normalize);
        }

        return { };
    }

    // We need a special case when the user changes the light type, we need to repopulate the prim
    void CreateCallbacks() override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
            .Msg("Creating light adapter callbacks for prim (%s).\n", GetID().GetText());

        MStatus status;
        auto    dag = GetDagPath();
        auto    obj = dag.node();
        
        // This is what is new compared to MayaHydraLightAdapter::CreateCallbacks()
        auto    id  = MNodeMessage::addAttributeChangedCallback(obj, _lightShapeChangedCallBack, this, &status);
        if (status) {
            AddCallback(id);
        }

        //This is the same as in MayaHydraLightAdapter::CreateCallbacks()
        id = MNodeMessage::addNodeDirtyCallback(obj, _dirtyParams, this, &status);
        if (status) {
            AddCallback(id);
        }
        

        dag.pop();
        for (; dag.length() > 0; dag.pop()) {
            // The adapter itself will free the callbacks, so we don't have to worry
            // about passing raw pointers to the callbacks. Hopefully.
            obj = dag.node();
            if (obj != MObject::kNullObj) {
                id = MNodeMessage::addAttributeChangedCallback(
                    obj, _changeVisibility, this, &status);
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

    
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<aiAreaLightAdapter, TfType::Bases<MayaHydraLightAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, aiAreaLight)
{
    MayaHydraAdapterRegistry::RegisterLightAdapter(
        TfToken("aiAreaLight"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag) -> MayaHydraLightAdapterPtr {
            return MayaHydraLightAdapterPtr(new aiAreaLightAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
