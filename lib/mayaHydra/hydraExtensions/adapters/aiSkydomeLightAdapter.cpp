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
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hio/image.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/tokens.h>
#include <pxr/base/tf/getenv.h>

#include <maya/MPlugArray.h>

#include <algorithm>
#include <cstdio>
#include <deque>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief MayaHydraAiSkyDomeLightAdapter is used to handle the translation from an Arnold skydome
 * light to hydra.
 */
class MayaHydraAiSkyDomeLightAdapter : public MayaHydraLightAdapter
{
    /// Temp folder path from the OS
    static std::string _tmpFolderPath;

    /// The 1x1 constant color textures written for this light, which Hydra uses when
    /// nothing is connected to the color, as it ignores the color of a dome light
    /// without a texture. One per color, so that a renderer caching textures by
    /// path sees a color edit. The render thread may still load the previous color's
    /// path from its copy of the scene, so the newest two are kept, most recent
    /// last; older ones are deleted, and the rest with the adapter.
    static constexpr size_t _kKeptConstantColorTextures = 2;
    std::deque<std::string> _constantColorTextures;

    static bool _IsConnected(const MPlug& plug)
    {
        MPlugArray conns;
        plug.connectedTo(conns, true, false);
        return conns.length() > 0;
    }

    static GfVec3f _GetColor(const MPlug& plug)
    {
        GfVec3f color(0.5f, 0.5f, 0.5f);
        if (!plug.isNull()) {
            plug.child(0).getValue(color[0]);
            plug.child(1).getValue(color[1]);
            plug.child(2).getValue(color[2]);
        }
        return color;
    }

    // Writes with Hio rather than VP2's texture manager: Hydra reads light parameters
    // on any thread, and the texture manager makes GL calls on whatever context is
    // current.
    std::string _GetConstantColorTexture(const GfVec3f& color)
    {
        if (_tmpFolderPath.empty()) {
            return {};
        }
        unsigned char texData[4];
        for (int i = 0; i < 3; ++i) {
            texData[i] = static_cast<unsigned char>(255 * std::clamp(color[i], 0.0f, 1.0f));
        }
        texData[3] = 255;

        const std::string path = _tmpFolderPath
            + TfStringPrintf("/HydraAiSkyDomeLightTex__%p__%02x%02x%02x__tmp.png",
                             this, texData[0], texData[1], texData[2]);
        const auto known
            = std::find(_constantColorTextures.begin(), _constantColorTextures.end(), path);
        if (known != _constantColorTextures.end()) {
            _constantColorTextures.erase(known);
            _constantColorTextures.push_back(path);
            return path;
        }
        HioImageSharedPtr image = HioImage::OpenForWriting(path);
        HioImage::StorageSpec storage;
        storage.width = 1;
        storage.height = 1;
        storage.depth = 1;
        storage.format = HioFormatUNorm8Vec4;
        storage.data = texData;
        if (!image || !image->Write(storage)) {
            return {};
        }
        _constantColorTextures.push_back(path);
        while (_constantColorTextures.size() > _kKeptConstantColorTextures) {
            std::remove(_constantColorTextures.front().c_str());
            _constantColorTextures.pop_front();
        }
        return path;
    }

public:
    MayaHydraAiSkyDomeLightAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag)
        : MayaHydraLightAdapter(mayaHydraSceneIndex, dag)
    {
        if (_tmpFolderPath.empty()) {
            std::string tmpDir = TfGetenv("TMPDIR");
            if (!tmpDir.empty()) {
                _tmpFolderPath = tmpDir;
            }
            else
            {
                tmpDir = TfGetenv("TEMP");
                if (!tmpDir.empty()) {
                    _tmpFolderPath = tmpDir;
                }
            }
        }
    }

    virtual ~MayaHydraAiSkyDomeLightAdapter()
    {
        for (const std::string& path : _constantColorTextures) {
            std::remove(path.c_str());
        }
    }

    const TfToken& LightType() const override { return HdPrimTypeTokens->domeLight; }

    TfTokenVector GetLightParamNames() const override
    {
        TfTokenVector names = MayaHydraLightAdapter::GetLightParamNames();
        names.insert(
            names.end(),
            { HdLightTokens->textureFormat,
              UsdLuxTokens->inputsTextureFormat,
              HdLightTokens->textureFile,
              UsdLuxTokens->inputsTextureFile });
        return names;
    }

    VtValue GetLightParamValue(const TfToken& paramName) override
    {
        MayaHydra::DgAccessLock dgLock;

        MStatus           status;
        MFnDependencyNode light(GetNode(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return {};
        }

        // We are not using precomputed attributes here, because we don't have
        // a guarantee that mtoa will be loaded before mayaHydra.
        if (paramName == HdLightTokens->color || paramName == UsdLuxTokens->inputsColor) {
            const MPlug plug = light.findPlug("color", true);
            if (_IsConnected(plug)) {
                return VtValue(
                    GfVec3f(1.0f, 1.0f, 1.0f)); // When there is a connection, return a white color.
            }
            return VtValue(_GetColor(plug));
        } else if (
            paramName == HdLightTokens->textureFormat
            || paramName == UsdLuxTokens->inputsTextureFormat) {
            const auto format = light.findPlug("format", true).asShort();
            // mirrored_ball : 0
            // angular : 1
            // latlong : 2
            if (format == 0) {
                return VtValue(UsdLuxTokens->mirroredBall);
            } else if (format == 2) {
                return VtValue(UsdLuxTokens->latlong);
            } else {
                return VtValue(UsdLuxTokens->automatic);
            }
        } else if (
            paramName == HdLightTokens->textureFile
            || paramName == UsdLuxTokens->inputsTextureFile) {
            // Be aware that dome lights in HdStorm always need a texture to work correctly,
            // the color is not used if no texture is present.

            const MPlug colorPlug = light.findPlug("color", true);
            if (!_IsConnected(colorPlug)) {
                const std::string path = _GetConstantColorTexture(_GetColor(colorPlug));
                // SdfAssetPath requires both "path" and "resolvedPath". An empty one
                // produces a warning, but means the texture could not be written.
                return VtValue(path.empty() ? SdfAssetPath() : SdfAssetPath(path, path));
            }

            const std::string domeLightTexturePath = MAYAHYDRA_NS::GetDomeLightTexture(
                light); // Using mayaUtils.h, the resulting string could be empty

            // SdfAssetPath requires both "path" "resolvedPath"
            return VtValue(SdfAssetPath(domeLightTexturePath, domeLightTexturePath));

        } else if (
            paramName == HdLightTokens->enableColorTemperature
            || paramName == UsdLuxTokens->inputsEnableColorTemperature) {
            return VtValue(false); // Needs to be false as Arnold sky dome light has no color
                                   // temperature attribute
        }

        return MayaHydraLightAdapter::GetLightParamValue(paramName);
    }
};

// Static variables from MayaHydraAiSkyDomeLightAdapter
std::string MayaHydraAiSkyDomeLightAdapter::_tmpFolderPath;

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraAiSkyDomeLightAdapter, TfType::Bases<MayaHydraLightAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, domeLight)
{
    MayaHydraAdapterRegistry::RegisterLightAdapter(
        TfToken("aiSkyDomeLight"),
        [](MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MDagPath&      dag) -> MayaHydraLightAdapterPtr {
            return MayaHydraLightAdapterPtr(
                new MayaHydraAiSkyDomeLightAdapter(mayaHydraSceneIndex, dag));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE