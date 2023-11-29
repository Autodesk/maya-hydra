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

#ifndef FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_INTERFACE_H
#define FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_INTERFACE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxyFwd.h"
#include "flowViewport/API/fvpFilteringSceneIndexClientFwd.h"

//STL headers
#include <string>

namespace FVP_NS_DEF
{
    /**
    * Interface to register a callbacks FilteringSceneIndexClient and append custom filtering scene indices to Hydra viewports scene indices.
    * To get an instance of the FilteringSceneIndexInterface class, please use :
    * Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    * 
    * The filtering scene indices added to a hydra viewport will act on all kind of data : DCC native data, USD stages and custom primitives added by data producer scene indices.
    */
    class FilteringSceneIndexInterface
    {
     public:

         ///Interface accessor
        static FVP_API FilteringSceneIndexInterface& get();

        /**
        *  @brief      Register a callbacks SceneIndexClient instance
        *
        *  @param[in]  client is a FilteringSceneIndexClient.
        * 
        *  @return     true if it succeded, false otherwise like the client if already registered.
        */
        virtual bool registerFilteringSceneIndexClient(FilteringSceneIndexClient& client) = 0;
        
        /**
        *  @brief      Unregister an SceneIndexClient instance
        *
        *              Unregister an SceneIndexClient instance, to stop receiving notifications.
        * 
        *  @param[in]  client is the FilteringSceneIndexClient to remove.
        */
        virtual void unregisterFilteringSceneIndexClient(FilteringSceneIndexClient& client)= 0;
    };

}//end of namespace

#endif //FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_INTERFACE_H
