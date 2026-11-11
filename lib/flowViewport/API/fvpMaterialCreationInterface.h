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

#ifndef FLOW_VIEWPORT_API_MATERIAL_CREATION_INTERFACE_H
#define FLOW_VIEWPORT_API_MATERIAL_CREATION_INTERFACE_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/base/vt/value.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <string>

namespace FVP_NS_DEF
{
    /*!\class MaterialCreationInterface
    \brief : interface for a customer to register named callbacks that create the Hydra material of a DCC material.
    * In MayaHydra, for Maya surface shaders of type GLSLShader, the callback registered under the shader file name
    * without directory and extension (from the "shader" attribute, e.g. "C:/shaders/myShader.ogsfx" gives "myShader")
    * is called.
    * To get an instance of the MaterialCreationInterface class, please use :
    * Fvp::MaterialCreationInterface& materialCreationInterface = Fvp::MaterialCreationInterface::Get();
    */
    class MaterialCreationInterface
    {
     public:

        /**
        *  @brief      Callback creating a Hydra material.
        *
        *  @param[in]  materialId is the Hydra path of the material prim.
        *
        *  @return     The Hydra material resource (an HdMaterialNetworkMap in a VtValue), or an empty VtValue
        *              to let the DCC use its default material conversion.
        */
        using MaterialCreationCallback = std::function<PXR_NS::VtValue(const PXR_NS::SdfPath& materialId)>;

        ///Interface accessor
        static FVP_API MaterialCreationInterface& Get();

        /**
        *  @brief      Register a material creation callback under a name, it replaces any callback previously
        *              registered under the same name.
        *
        *  @param[in]  name is the name the callback is registered under.
        *  @param[in]  callback is the material creation callback.
        */
        virtual void RegisterMaterialCreationCallback(const std::string& name, const MaterialCreationCallback& callback) = 0;

        /**
        *  @brief      Unregister the material creation callback registered under a name, the DCC default material
        *              conversion is used again for that name. Callbacks registered under other names are kept.
        *
        *  @param[in]  name is the name the callback was registered under.
        */
        virtual void UnregisterMaterialCreationCallback(const std::string& name) = 0;

        /**
        *  @brief      Call the material creation callback registered under a name.
        *
        *  @param[in]  name is the name the callback was registered under.
        *  @param[in]  materialId is the Hydra path of the material prim.
        *
        *  @return     The Hydra material resource created by the callback, or an empty VtValue if no callback
        *              is registered under that name.
        */
        virtual PXR_NS::VtValue CreateMaterial(const std::string& name, const PXR_NS::SdfPath& materialId) const = 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_MATERIAL_CREATION_INTERFACE_H
