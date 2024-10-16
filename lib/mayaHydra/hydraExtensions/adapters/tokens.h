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
#ifndef MAYAHYDRALIB_ADAPTER_TOKENS_H
#define MAYAHYDRALIB_ADAPTER_TOKENS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/tf/staticTokens.h>
#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

/**
 * \brief MayaHydraAdapterTokens contains all the hydra tokens used by this plug-in.
 */

// clang-format off
#define MAYAHYDRALIB_ADAPTER_TOKENS           \
    (roughness)                               \
    (clearcoat)                               \
    (clearcoatRoughness)                      \
    (emissiveColor)                           \
    (specular)                                \
    (specularColor)                           \
    (metallic)                                \
    (useSpecularWorkflow)                     \
    (occlusion)                               \
    (ior)                                     \
    (normal)                                  \
    (opacity)                                 \
    (diffuse)                                 \
    (diffuseColor)                            \
    (displacement)                            \
    (base)                                    \
    (baseColor)                               \
    (emission)                                \
    (emissionColor)                           \
    (metalness)                               \
    (specularIOR)                             \
    (specularRoughness)                       \
    (coat)                                    \
    (coatRoughness)                           \
    (transmission)                            \
    (lambert)                                 \
    (blinn)                                   \
    (phong)                                   \
    (standardSurface)                         \
    (file)                                    \
    (place2dTexture)                          \
    (fileTextureName)                         \
    (color)                                   \
    (incandescence)                           \
    (out)                                     \
    (output)                                  \
    (st)                                      \
    (uv)                                      \
    (uvCoord)                                 \
    (rgb)                                     \
    (r)                                       \
    (xyz)                                     \
    (x)                                       \
    (varname)                                 \
    (result)                                  \
    (eccentricity)                            \
    (usdPreviewSurface)                       \
    (pxrUsdPreviewSurface)                    \
    (MayaHydraLambertShader)                  \
    (MayaHydraPhongShader)                    \
    (MayaHydraBlinnShader)                    \
	(MayaHydraStippleShader)                  \
    (MayaHydraSolidColorShader)               \
    (tangents)

// clang-format on

TF_DECLARE_PUBLIC_TOKENS(MayaHydraAdapterTokens, MAYAHYDRALIB_API, MAYAHYDRALIB_ADAPTER_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_ADAPTER_TOKENS_H
