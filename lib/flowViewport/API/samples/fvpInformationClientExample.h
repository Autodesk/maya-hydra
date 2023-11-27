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
#ifndef FLOW_VIEWPORT_EXAMPLES_INFORMATION_CLIENT_EXAMPLE_H
#define FLOW_VIEWPORT_EXAMPLES_INFORMATION_CLIENT_EXAMPLE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpInformationClient.h"

namespace FVP_NS_DEF {

class FilteringSceneIndexClientExample;//Predeclaration

///Implementation of a FlowViewport::InformationClient which is the way to communicate with our Hydra plugin related to viewports information
class FVP_API InformationClientExample : public InformationClient
{
public:
    InformationClientExample() = default;
    ~InformationClientExample() override;

    ///Set the hydra interface
    void SetHydraInterface(InformationInterface* hydraInterface) {_hydraInterface = hydraInterface;}

    /// set the filtering scene index client example
    void SetFilteringSceneIndexClientExample(FilteringSceneIndexClientExample& hydraViewportFilteringSceneIndexClientExample)
                {_hydraViewportFilteringSceneIndexClientExample = &hydraViewportFilteringSceneIndexClientExample;}

    //From FlowViewport::InformationClient

    /**
    *  @brief      Callback function called when a Hydra viewport scene index is being created by our Hydra viewport plugin
    *
    *              This is a callback function that gets called when a Hydra viewport scene index is being created by our Hydra viewport plugin. 
    *              A typical case is when a Hydra viewport is created.
    * 
    *  @param[in]  viewportInformation is a viewport information from the scene index being added by our Hydra viewport plugin.
    */
    void SceneIndexAdded(const InformationInterface::ViewportInformation& viewportInformation)override;

    /**
    *  @brief      Callback function called when a Hydra viewport scene index is being removed by our Hydra viewport plugin
    *
    *              This is a callback function that gets called when a Hydra viewport scene index is removed by our Hydra viewport plugin. 
    *              A typical case is when a Hydra viewport is removed.
    * 
    *  @param[in]  viewportInformation is a viewport information from the scene index being removed by our Hydra viewport plugin.
    */
    void SceneIndexRemoved(const InformationInterface::ViewportInformation& viewportInformation)override;

protected:
    ///Store the Hydra interface
    InformationInterface* _hydraInterface = nullptr;

    ///Store an FilteringSceneIndexClientExample since it needs to be aware of the viewports being removed
    FilteringSceneIndexClientExample* _hydraViewportFilteringSceneIndexClientExample = nullptr;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_EXAMPLES_INFORMATION_CLIENT_EXAMPLE_H
