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
            ViewportInformation(const PXR_NS::HdSceneIndexBaseRefPtr& viewportSceneIndex, const std::string& cameraName, const size_t viewportWidth, const size_t viewportHeight, const std::string& rendererName)
                : _viewportSceneIndex(viewportSceneIndex), _cameraName(cameraName), _viewportWidth(viewportWidth), _viewportHeight(viewportHeight), _rendererName(rendererName) {}
            
            ///_viewportSceneIndex is an Hydra viewport scene index
            const PXR_NS::HdSceneIndexBaseRefPtr _viewportSceneIndex;

            ///_cameraName is the name of the camera/viewport when the viewport was created, it is not updated if the camera's name has changed.
            const std::string _cameraName;

            ///_viewportWidth is the viewport width when the viewport was created, it is not updated if the viewport's size has changed.
            const int _viewportWidth = 0; 

            ///_viewportHeight is the viewport height when the viewport was created, it is not updated if the viewport's size has changed.
            const int _viewportHeight = 0; 

            ///_rendererName is the Hydra viewport renderer name (example : "GL" for Storm or "Arnold" for the Arnold render delegate)
            const std::string _rendererName;
        };

        /**
        *  @brief      Register a set of callbacks through an InformationClient instance
        *
        *  @param[in]  client is the InformationClient.
        */
        virtual void RegisterInformationClient(const InformationClient& client) = 0;
        
        /**
        *  @brief      Unregister an InformationClient instance
        *
        *  @param[in]  client is the InformationClient.
        */
        virtual void UnregisterInformationClient(const InformationClient& client)= 0;


        /**
        *  @brief      Get the Hydra viewports information. 
        *
        *  @param[out] outHydraViewportInformationSet is a set of ViewportInformation to have information about each Hydra viewport in use in the current DCC.
        */
        virtual void GetViewportsInformation(std::set<const ViewportInformation*>& outHydraViewportInformationSet)const  = 0;
    };
    
    ///Set of InformationInterface::ViewportInformation
    using ViewportInformationSet = std::set<const InformationInterface::ViewportInformation*>;

}//end of namespace

#endif //FLOW_VIEWPORT_API_INFORMATION_INTERFACE_H
