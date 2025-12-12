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
    \brief : interface for a customer to register a callbacks InformationClient to get Hydra render view information.
    * To get an instance of the InformationInterface class, please use :
    * Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    */
    class InformationInterface
    {
     public:
        
        ///Interface accessor
        static FVP_API InformationInterface& Get();

        ///Descriptor struct used to store information about a Hydra render view from the DCC
        struct RenderViewDesc
        {
            /// Constructor
            RenderViewDesc(const std::string& viewId, bool isViewport)
                : _viewId(viewId), _isViewport(isViewport) {}
            
            /// Identifier which is unique for all Hydra render views during a session
            std::string _viewId;

            /// Marks whether this render view is an interactive viewport or not
            bool _isViewport;

            /// Hydra renderer name (example : "GL" for Storm or "Arnold" for the Arnold render delegate)
            std::string _rendererName;

            /**
             *  @brief  Assignment operator.
             */
            RenderViewDesc& operator = (const RenderViewDesc& other){
                _viewId = other._viewId;
                _rendererName = other._rendererName;
                return *this;
            }
            
            /**
             *  @brief  Equal operator.
             *  @return true if the RenderViewDesc are identical.
             */
            bool operator ==(const RenderViewDesc& other)const{
                return  _viewId == other._viewId &&
                        _rendererName == other._rendererName;
            }

            /**
             *  @brief  lower than operator for containers ordering.
             *  @return true if the RenderViewDesc sent as a parameter is considered as lower.
             */
            bool operator <(const RenderViewDesc& other) const{ //to be used in std::set
                auto a = {_viewId, _rendererName};
                auto b = {other._viewId, other._rendererName};
                return std::lexicographical_compare(a.begin(), a.end(), b.begin(), b.end());
            }
        };

        ///Set of InformationInterface::RenderViewDesc
        typedef std::set<InformationInterface::RenderViewDesc> RenderViewDescSet;

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
        *  @brief      Get all the Hydra render view descriptors. 
        *
        *  @param[out] outRenderViewDescs is a set of RenderViewDesc to have information about each Hydra render view in use in the current DCC.
        */
        virtual void GetAllRenderViewDescs(RenderViewDescSet& outRenderViewDescs)const  = 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_INFORMATION_INTERFACE_H
