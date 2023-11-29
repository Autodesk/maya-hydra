//
// Copyright 2023 Autodesk
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
#ifndef FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_CLIENT_EXAMPLE_H
#define FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_CLIENT_EXAMPLE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpFilteringSceneIndexClient.h"
#include "flowViewport/API/fvpFilteringSceneIndexInterface.h"
#include "flowViewport/API/samples/fvpFilteringSceneIndexExample.h"

namespace FVP_NS_DEF {

/** This class is an implementation of FilteringSceneIndexClient which is an example on how to filter Hydra primitives from the scene into a Hydra viewport. 
*   It will use the scene index filter from the class FilteringSceneIndexExample.
* 
*   Usage of this class is :
* 
*   Call FilteringSceneIndexInterface::registerFilteringSceneIndexClient with this instance with for example :
*   FilteringSceneIndexClientExample hydraViewportFilteringSceneIndexClient ("FilteringSceneIndexClientExample", 
*                                            Fvp::FilteringSceneIndexClient::Bucket::kSceneFiltering, 
*                                            FvpViewportAPITokens->allRenderers, //We could set only Storm by using "GL" or only Arnold by using "Arnold" or both with "GL, Arnold"
*                                            nullptr);//No node associated, you could still do it later though.
*   //Register a filtering scene index client
*   Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
*   //Register this filtering scene index client, so it can append custom filtering scene indices to Hydra viewport scene indices
*   const bool bResult = filteringSceneIndexInterface.registerFilteringSceneIndexClient(hydraViewportFilteringSceneIndexClient);
*
*   The callback FilteringSceneIndexClientExample::appendSceneIndex will be called when a new viewport is created to append your filtering scene index.
* 
*   If you want to unregister your client, please use :
*   Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
*   filteringSceneIndexInterface.unregisterFilteringSceneIndexClient(hydraViewportFilteringSceneIndexClient);
*/

class FVP_API FilteringSceneIndexClientExample : public FilteringSceneIndexClient
{
public:
    /// Constructor, please see below for the meaning of the parameters
    FilteringSceneIndexClientExample(const std::string& displayName, const Bucket bucket, const std::string& rendererNames, void* dccNode);
    /// Destructor
    ~FilteringSceneIndexClientExample() override = default;

    ///Is called by the DCC's node to set its node pointer, used later in the hydra viewport API
    void setDccNode(void* node){_dccNode = node;}

    //From FilteringSceneIndexClient

    /**
    *  @brief      Callback function to append a scene index.
    *
    *               This callback function gets called for you to append a scene index to a Hydra viewport scene index, like a filtering scene index. 
    *               A typical case is when a new Hydra viewport is created, after some internal managment of this scene index, we call this function so you can append one scene index 
    *               or a chain of scene indices and return the last element of the chain.
    *               The returned value of this function is the last custom scene index of a a chain that you want to append to this scene index, 
    *               or just return the input scene index passed if you don't want to append any scene index.
    * 
    *  @param[in]      inputSceneIndex is a HdSceneIndexBaseRefPtr which was created by our Hydra viewport plugin. This could be the Hydra viewport scene index or it could be some appended
    *                  scene index, as a chain of scene indices is appended to the Hydra viewport scene index if several filtering scene index clients are registered.
    *                  So don't assume it's the Hydra viewport scene index.
    *  @param[in]      inputArgs is a container data source handle to deal with the possibility to send custom data from our Hydra viewport plugin for the creation of your scene index.
    *                  This parameter is currently not used by the Hydra viewport plugin but is left for possible future use.
    * 
    *  @return     If you don't want to append a scene index, just return _inputSceneIndex. 
    *              If you want to append a scene index or a scene indices chain, you should return the last scene index of the scene indices chain to append.
    */
    PXR_NS::HdSceneIndexBaseRefPtr appendSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const PXR_NS::HdContainerDataSourceHandle& inputArgs)override;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_CLIENT_EXAMPLE_H
