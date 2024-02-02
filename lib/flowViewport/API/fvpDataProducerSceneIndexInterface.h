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

#ifndef FLOW_VIEWPORT_API_DATA_PRODUCER_SCENE_INDEX_INTERFACE_H
#define FLOW_VIEWPORT_API_DATA_PRODUCER_SCENE_INDEX_INTERFACE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpViewportAPITokens.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF
{
    /**
    * Interface to manage data producer scene indices in a Hydra viewport. A data producer scene index is a scene index that adds primitives to the current rendering.
    * These new primitives are created without the need of a DCC object or a USD stage.
    * To get an instance of the DataProducerSceneIndexInterface class, please use :
    * Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    */
    class DataProducerSceneIndexInterface
    {
     public:
    
        /// Interface accessor
        static FVP_API DataProducerSceneIndexInterface& get();

        /**
        *  @brief       Adds a custom data producer scene index.
        *
        *               Adds a custom data producer scene index and associate it to be used in the same rendering as the hydra viewport whose identifier is hydraViewportId 
        *               (or all hydra viewports if hydraViewportId is DataProducerSceneIndexInterface::allViewports). 
        *               Basically, we merge this scene index with the others scene indices from the viewport which are the usd stages, the DCC native 
        *               data and any others custom data producer scene indices like this one.
        * 
        *  @param[in]   customDataProducerSceneIndex is the custom scene index to add.
        * 
        *  @param[in, out] inoutPreFix is the prefix you want to add to your data producer scene index primitives, it may be modified by this function if you provide a dccnode.
        *                  If you don't want any prefix, pass SdfPath::AbsoluteRootPath() to this parameter.
        * 
        *  @param[in]   dccNode is a MObject* from a DAG node for Maya, if you provide the pointer value, then we automatically track some events such as transform 
        *               or visibility updated and we hide automatically the primitives from the data producer scene index. 
        *               If it is a nullptr, we won't do anything if the node's attributes changes.
        *               Basically, this is a way for you to set the DCC node as a parent node for all your primitives from the scene index.
        * 
        *  @param[in]   hydraViewportId is a Hydra viewport string identifier to which customDataProducerSceneIndex needs to be associated to. 
        *               Set it to DataProducerSceneIndexInterface::allViewports to add this data producer scene index to all viewports. 
        *               To retrieve a specific hydra viewport identifier, please use the InformationInterface class.
        * 
        *  @param[in]  rendererNames : are the Hydra renderer names to which this scene index should be added.
        *              This is only used when hydraViewportId is set to DataProducerSceneIndexInterface::allViewports, meaning you want to add this scene index to all viewports 
        *              that are using these renderers.
        *              To apply to multiple renderers, use a separator such as ",". E.g : "GL, Arnold". We are actually looking for the render delegate's name in this string.
        *              Set this parameter to FvpViewportAPITokens->allRenderers to add your scene index to all viewports whatever their renderer is.
        * 
        *  @param[in]  customDataProducerSceneIndexRootPathForInsertion is the root path for insertion used as a second parameter of HdRenderIndex::InsertSceneIndex method.
        *              e.g : renderIndex.InsertSceneIndex(customDataProducerSceneIndex, customDataProducerSceneIndexRootPathForInsertion);
        * 
        *  @return     true if the operation succeeded, false otherwise.
        */
        virtual bool addDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                               PXR_NS::SdfPath& inoutPreFix,
                                               void* dccNode = nullptr,
                                               const std::string& hydraViewportId = PXR_NS::FvpViewportAPITokens->allViewports,
                                               const std::string& rendererNames = PXR_NS::FvpViewportAPITokens->allRenderers
                                              ) = 0;

        /**
        *  @brief      Removes a custom data producer scene index.
        *
        *              Removes a custom data producer scene index, this scene index will not participate any more to the rendering of the given viewport(s).
        * 
        *  @param[in]  customDataProducerSceneIndex is the custom scene index to remove.
        * 
        *  @param[in]  hydraViewportId is the hydra viewport string identifier to which customDataProducerSceneIndex was associated to or DataProducerSceneIndexInterface::allViewports
        *               if it was applied to all viewports.
        */
        virtual void removeViewportDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                          const std::string& hydraViewportId = PXR_NS::FvpViewportAPITokens->allViewports
                                                         ) = 0;
    };
    
}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_API_DATA_PRODUCER_SCENE_INDEX_INTERFACE_H
