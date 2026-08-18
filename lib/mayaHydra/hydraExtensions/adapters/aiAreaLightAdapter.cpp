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
#include <mayaHydraLib/adapters/dagAdapter.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/usd/usdLux/tokens.h>

#include <maya/MPlug.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief MayaHydraAiAreaLightAdapter is used to handle the translation from a Maya area light to
 * hydra.
 */
class MayaHydraAiAreaLightAdapter : public MayaHydraLightAdapter
{
public:
    MayaHydraAiAreaLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraLightAdapter(mayaHydraSceneIndex, dag)
    {
    }

    const TfToken& LightType() const override
    {
        const TfToken& defaultLightType = HdPrimTypeTokens->rectLight;

        MayaHydra::DgAccessLock dgLock;

        // Get the light type
        MStatus           status;
        MFnDependencyNode depNode(GetNode(), &status);
        if (!status) {
            return defaultLightType;
        }

        MString primitiveType = "quad";
        MPlug   plug = depNode.findPlug("aiTranslator", true);
        if (!plug.isNull()) {
            primitiveType = plug.asString();
            if (primitiveType.length() == 0) {
                return defaultLightType;
            }
        }

        if (primitiveType == "quad") {
            return defaultLightType;
        } else if (primitiveType == "disk") {
            return HdPrimTypeTokens->diskLight;
        } else if (primitiveType == "cylinder") {
            return HdPrimTypeTokens->cylinderLight;
        }

        return defaultLightType;
    }

    VtValue GetLightParamValue(const TfToken& paramName) override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET_LIGHT_PARAM_VALUE)
            .Msg(
                "Called MayaHydraAiAreaLightAdapter::GetLightParamValue(%s) - %s\n",
                paramName.GetText(),
                GetDagPath().partialPathName().asChar());

        MayaHydra::DgAccessLock dgLock;

        MStatus           status;
        MFnDependencyNode depNode(GetNode(), &status);
        if (!status) {
            return {};
        }

        // the aiAreaLight has no width/height/radius/length attributes, so we need to calculate
        // them from the scale
        MString primitiveType = "quad";
        double  scale2[3] = { 1.0, 1.0, 1.0 };

        MPlug                 plug = depNode.findPlug("aiTranslator", true);
        MTransformationMatrix modelMatrix(GetDagPath().inclusiveMatrix());
        if (!plug.isNull()) {
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

        constexpr float defaultWidth
            = 2.0f; // By default the drawing of the light shape has a width and height of 2.0
        constexpr float defaultHeight = 2.0f;

        // The width, height, length, radius are only queried by Hydra if the "normalize"
        // (aiNormalize below) attribute is unchecked
        if ((paramName == HdLightTokens->width) || (paramName == UsdLuxTokens->inputsWidth)) {
            return VtValue(float(defaultWidth * scale2[0]));
        } else if (
            (paramName == HdLightTokens->height) || (paramName == UsdLuxTokens->inputsHeight)) {
            return VtValue(float(defaultHeight * scale2[1]));
        } else if (
            (paramName == HdLightTokens->radius) || (paramName == UsdLuxTokens->inputsRadius)) {
            return VtValue(float(scale2[0]));
        } else if (
            (paramName == HdLightTokens->length) || (paramName == UsdLuxTokens->inputsLength)) {
            return VtValue(float(scale2[0]));
        } else if (
            GetMayaHydraSceneIndex()->IsHdSt() //Storm will use shadow maps
            &&(
                (paramName == HdLightTokens->shadowEnable)  || 
                (paramName == HdLightTokens->hasShadow)     || 
                (paramName == UsdLuxTokens->inputsShadowEnable)
              )
            ){
            // From a comment in OpenUSD : Shadow maps are supported only for SimpleLights and
            // DistantLights
            // https://github.com/PixarAnimationStudios/OpenUSD/blob/8843f3b7b334bbcd8df014e63d1b8fad24fc6b6e/pxr/imaging/hdx/shadowTask.cpp#L117
            return VtValue(false); // No shadows for Storm with aiAreaLight
        }

        return MayaHydraLightAdapter::GetLightParamValue(paramName);
    }

    bool OnShapeAttributeChanged(const MPlug& plug) override
    {
        if (plug.partialName() == MString("ai_translator")) {
            RemovePrim();
            Populate();
            InvalidateTransform();
            return true;
        }
        return MayaHydraLightAdapter::OnShapeAttributeChanged(plug);
    }
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraAiAreaLightAdapter, TfType::Bases<MayaHydraLightAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, aiAreaLightMayaHydra)
{
    MayaHydraAdapterRegistry::RegisterLightAdapter(
        TfToken("aiAreaLight"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MDagPath&      dag) -> MayaHydraLightAdapterPtr {
            return MayaHydraLightAdapterPtr(
                new MayaHydraAiAreaLightAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE