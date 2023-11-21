//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#ifndef FLOW_VIEWPORT_API_INFORMATION_INTERFACE_H
#define FLOW_VIEWPORT_API_INFORMATION_INTERFACE_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF
{
    class InformationClient;//Predeclaration

    /*!\class InformationInterface 
    \brief : interface for a customer to register a callbacks InformationClient to get Hydra viewports information.
    * To get an instance of the InformationInterface class, please use :
    * Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    */
    class InformationInterface
    {
     public:
        
        ///Interface accessor
        static FVP_API InformationInterface& Get();

        ///Struct used to store information about an Hydra viewport from the DCC
        struct ViewportInformation
        {
            /// Constructor
            ViewportInformation(const std::string& viewportId, const std::string& cameraName)
                : _viewportId(viewportId), _cameraName(cameraName) {}
            
            ///_viewportId is an Hydra viewport string identifier which is unique for all hydra viewports during a session
            const std::string _viewportId;

            ///_cameraName is the name of the camera/viewport when the viewport was created, it is not updated if the camera's name has changed.
            const std::string _cameraName;

            ///_rendererName is the Hydra viewport renderer name (example : "GL" for Storm or "Arnold" for the Arnold render delegate)
            std::string _rendererName;

            bool operator ==(const ViewportInformation& other)const{
                return  _viewportId == other._viewportId &&
                        _cameraName == other._cameraName &&
                        _rendererName == other._rendererName;
            }

            bool operator <(const ViewportInformation& other) const{ //to be used in std::set
                auto a = {_viewportId, _cameraName, _rendererName};
                auto b = {other._viewportId, other._cameraName, other._rendererName};
                return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
            }
        };

        ///Set of InformationInterface::ViewportInformation
        typedef std::set<InformationInterface::ViewportInformation> ViewportInformationSet;

        /**
        *  @brief      Register a set of callbacks through an InformationClient instance
        *
        *  @param[in]  client is the InformationClient.
        */
        virtual void RegisterInformationClient(const std::shared_ptr<InformationClient>& client) = 0;
        
        /**
        *  @brief      Unregister an InformationClient instance
        *
        *  @param[in]  client is the InformationClient.
        */
        virtual void UnregisterInformationClient(const std::shared_ptr<InformationClient>& client)= 0;

        /**
        *  @brief      Get the Hydra viewports information. 
        *
        *  @param[out] outAllHydraViewportInformation is a set of ViewportInformation to have information about each Hydra viewport in use in the current DCC.
        */
        virtual void GetViewportsInformation(ViewportInformationSet& outAllHydraViewportInformation)const  = 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_INFORMATION_INTERFACE_H
