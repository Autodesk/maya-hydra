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
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/usd/usdLux/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief MayaHydraAreaLightAdapter is used to handle the translation from a Maya area light to
 * hydra.
 */
class MayaHydraAreaLightAdapter : public MayaHydraLightAdapter
{
public:
    MayaHydraAreaLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraLightAdapter(mayaHydraSceneIndex, dag)
    {
    }

    void _CalculateLightParams(GlfSimpleLight& light) override { light.SetSpotCutoff(90.0f); }

    const TfToken& LightType() const override
    {
        return HdPrimTypeTokens->rectLight;
    }

    VtValue GetLightParamValue(const TfToken& paramName) override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET_LIGHT_PARAM_VALUE)
            .Msg(
                "Called MayaHydraAreaLightAdapter::GetLightParamValue(%s) - %s\n",
                paramName.GetText(),
                GetDagPath().partialPathName().asChar());

        auto sizeScaled = [=](int index) {
            constexpr float defaultSizeForAreaLights[2] { 2.0f, 2.0f };
            double                      scale[3] = { 1.0, 1.0, 1.0 };
            const MTransformationMatrix modelMatrix(GetDagPath().inclusiveMatrix());
            modelMatrix.getScale(scale, MSpace::kWorld);
            const float sizeScaled = defaultSizeForAreaLights[index] * scale[index];
            return VtValue(sizeScaled);
        };

        //To increase the width or height of a maya area light, you need to scale the shape
        // so apply the scaling factor to the default width and height of 2
        if ((paramName == HdLightTokens->width)
            || (paramName == UsdLuxTokens->inputsWidth)){
            constexpr int widthIndex = 0;
            return sizeScaled(widthIndex);
        } else if ( (paramName == HdLightTokens->height) 
            ||      (paramName == UsdLuxTokens->inputsHeight) ) {
            constexpr int heightIndex = 1;
            return sizeScaled(heightIndex);
        } else if  ((paramName == HdLightTokens->intensity) 
                ||  (paramName == UsdLuxTokens->inputsIntensity) ){
            // Override intensity to match VP2
            MStatus           status;
            MFnDependencyNode lightDepNode(GetNode(), &status);
            if (status == MS::kSuccess) {
                MPlug intensityPlug = lightDepNode.findPlug("intensity", true, &status);
                if (status == MS::kSuccess && !intensityPlug.isNull()) {
                    float overidenIntensity = intensityPlug.asFloat();
                    if (GetMayaHydraSceneIndex()->IsHdSt()) {
                        overidenIntensity /= M_PI;//For Storm only
                    }
                    return VtValue(overidenIntensity);
                }
            }
        }else if (
                (paramName == HdLightTokens->shadowEnable)
                || (paramName == HdLightTokens->hasShadow)
                || (paramName == UsdLuxTokens->inputsShadowEnable))
            {
                // From a comment in OpenUSD : Shadows are supported on for SimpleLights and
                // DistantLights
                return VtValue(false); // No shadows
            }

        return MayaHydraLightAdapter::GetLightParamValue(paramName);
    }
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraAreaLightAdapter, TfType::Bases<MayaHydraLightAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, areaLight)
{
    MayaHydraAdapterRegistry::RegisterLightAdapter(
        TfToken("areaLight"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag) -> MayaHydraLightAdapterPtr {
            return MayaHydraLightAdapterPtr(new MayaHydraAreaLightAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
