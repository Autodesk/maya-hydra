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

#ifndef FLOW_VIEWPORT_API_SELECTION_INTERFACE_H
#define FLOW_VIEWPORT_API_SELECTION_INTERFACE_H

#include "flowViewport/api.h"

namespace FVP_NS_DEF
{
    class SelectionClient;//Predeclaration
    
    /** 
    * FlowSelectionInterface is used to register/unregister selection callbacks SelectionClient for a Hydra viewport.
    * To get an instance of the FlowSelectionInterface class, please use :
    * Fvp::FlowSelectionInterface& flowSelectionInterface = Fvp::FlowSelectionInterface::Get();
    */
    class FlowSelectionInterface
    {
     public:
        
         /**
        *  @brief      Access to this interface
        *
        *  @return     A reference on the FlowSelectionInterface
        */
         static FVP_API FlowSelectionInterface& Get();
        
        /**
        *  @brief      Register a callbacks SelectionClient instance
        *
        *  @param[in]  client is the SelectionClient instance you are registering.
        */
        virtual void RegisterSelectionClient(SelectionClient& client) = 0;
        
        /**
        *  @brief      Unregister a callbacks SelectionClient instance
        *
        *  @param[in]  client is an SelectionClient instance you have previously registered and want to unregister.
        */
        virtual void UnregisterSelectionClient(SelectionClient& client)= 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_SELECTION_INTERFACE_H
