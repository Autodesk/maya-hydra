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


/// Is the definition of a callbacks InformationClient for a Hydra viewport.

#ifndef FLOW_VIEWPORT_API_INFORMATION_CLIENT_H
#define FLOW_VIEWPORT_API_INFORMATION_CLIENT_H

#include "flowViewport/api.h"

#include "fvpInformationInterface.h"

namespace FVP_NS_DEF
{
    ///Subclass this to create a client and register it through the InformationInterface class 
    class InformationClient
    {
    public:
        
        /**
        *  @brief      Callback function called when a Hydra viewport scene index is being created by our Hydra viewport plugin
        *
        *             This is a callback function that gets called when a Hydra viewport scene index is being created by our Hydra viewport plugin. 
        *              A typical case is when a Hydra viewport is created.
        * 
        *  @param[in]  viewportInformation is a Hydra viewport information from the scene index being added by our Hydra viewport plugin.
        */
        virtual void SceneIndexAdded(const InformationInterface::ViewportInformation& viewportInformation) = 0;
        
        /**
        *  @brief      Callback function called when a Hydra viewport scene index is being removed by our Hydra viewport plugin
        *
        *              This is a callback function that gets called when a Hydra viewport scene index is removed by our Hydra viewport plugin. 
        *              A typical case is when a Hydra viewport is removed.
        * 
        *  @param[in]  viewportInformation is a Hydra viewport information from the scene index being removed by our Hydra viewport plugin.
        */
        virtual void SceneIndexRemoved(const InformationInterface::ViewportInformation& viewportInformation) = 0;

        /// Destructor
        virtual ~InformationClient() = default;
    };

    /// Set of InformationClient
    using SharedInformationClientPtrSet = std::set<std::shared_ptr<InformationClient>>;

}//end of namespace

#endif //FLOW_VIEWPORT_API_INFORMATION_CLIENT_H
