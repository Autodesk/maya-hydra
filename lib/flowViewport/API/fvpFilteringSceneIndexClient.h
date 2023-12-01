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


/// Is the definition of a customer Hydra client to register a set of callbacks for a Hydra viewport.
#ifndef FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_H
#define FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_H

#include "flowViewport/api.h"
#include "flowViewport/API/fvpViewportAPITokens.h"

#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF
{
    ///Subclass this to create a callbacks FilteringSceneIndexClient and register it through the FilteringSceneIndexInterface class 
    class FilteringSceneIndexClient
    {
    public:
        /**
         * A Category is a container in which you want your filtering scene index or scene index chain to go to.
         * The filtering scene indices inside a Category don't have any specific priority when they are called.
         */
        enum class Category
        {
            /// kSelectionHighlighting is to register a filtering scene index to do custom selection highlighting (still a WIP)
            kSelectionHighlighting,
            /** kSceneFiltering is to register a filtering scene index applied to the primitives from the scene, 
            *   including usd stages, DCC native objects and custom data producer scene indices primitives.
            */
            kSceneFiltering,
        };

        /**
        *  @brief      Callback function to append a scene index.
        *
        *               This callback function gets called for you to append a scene index to a Hydra viewport scene index, like a filtering scene index. 
        *               A typical case is when a new Hydra viewport is created, after some internal managment of this scene index, we call this function so you can append one scene index 
        *               or a chain of scene indices and return the last element of the chain.
        *               The returned value of this function is the last custom scene index of a a chain that you want to append to this scene index, 
        *               or just return the input scene index passed if you don't want to append any scene index.
        * 
        *  @param[in]      displayName is a display name to be associated with your plugin.
        *  @param[in]      category is the container in which you want your filtering scene index (or filtering scene index chain) to go into.
        *  @param[in]      rendererNames is the names of the renderers you want this client to be associated to.
        *                  If there are several, separate them with for example a coma, like "GL, Arnold", we actually look for the renderer name in this string.
        *                  If you want your client to work on any renderer please use FvpViewportAPITokens->allRenderers.
        *  @param[in]      dccNode is a MObject* for Maya, if you provide the pointer value, then we automatically track some events such as visibility changed, 
        *                  node deleted/undeleted and we remove/add automatically your filtering scene indices from the viewport. Meaning if the maya node is visible your filtering
        *                  scene indices are applied to the scene, if the node is not visible (or deleted) your filtering scene indices are removed from the scene.
        *                  If it is a nullptr, your filtering scene indices will stay applied to the viewport(s) until you remove them.
        * 
        *  @param[in]      inputArgs is a container data source handle to deal with the possibility to send custom data from our Hydra viewport plugin for the creation of your scene index.
        *                  This parameter is currently not used by the Hydra viewport plugin but is left for possible future use.
        */
        FilteringSceneIndexClient(const std::string& displayName, const Category category, const std::string& rendererNames, void* dccNode):
            _displayName{displayName}, _category{category}, _rendererNames{rendererNames}, _dccNode{dccNode}
        {}

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
        virtual PXR_NS::HdSceneIndexBaseRefPtr appendSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const PXR_NS::HdContainerDataSourceHandle& inputArgs) = 0;

        /// Destructor
        virtual ~FilteringSceneIndexClient() = default;

        /**
        *  @brief  Get the display name.
        *  @return the display name. 
        */
        const std::string& getDisplayName() const {return _displayName;}
        
        /**
        *  @brief  Get the Category.
        *  @return the Category. 
        */
        const Category getCategory() const {return _category;}

        /**
        *  @brief  Get the renderer names.
        *  @return the renderer names. 
        */
        const std::string& getRendererNames() const {return _rendererNames;}

        /**
        *  @brief  Set the dcc node.
        */
        void setDccNode(void* dccNode) {_dccNode = dccNode;}

        /**
        *  @brief  Get the dcc node.
        *  @return the dcc node. 
        */
        void* getDccNode() const {return _dccNode;}
        
        bool operator == (const FilteringSceneIndexClient& other)const 
        {
            return _displayName == other._displayName &&
                _category == other._category &&
                _rendererNames == other._rendererNames &&
                _dccNode == other._dccNode;
        }

     protected:
        /**_displayName is a display name to be associated with your plugin.
        */
        const std::string _displayName         {"Unnamed"};

        /**_category is the container in which you want your filtering scene index (or filtering scene index chain) to go into.
        */
        const Category _category   {Category::kSceneFiltering};

        /**_rendererNames is the names of the renderers you want this client to be associated to.
        *  If there are several, separate them with comas, like "GL, Arnold"
        *  If you want your client to work on any renderer please use FvpViewportAPITokens->allRenderers.
        */
        const std::string _rendererNames   = PXR_NS::FvpViewportAPITokens->allRenderers;

        /**_dccNode is a MObject* for Maya, if you provide the pointer value, then we automatically track some events such as visibility changed, 
        *  node deleted/undeleted and we remove/add automatically your filtering scene indices from the viewport. Meaning if the maya node is visible your filtering
        *  scene indices are applied to the scene, if the node is not visible (or deleted) your filtering scene indices are removed from the scene.
        *  If it is a nullptr, your filtering scene indices will stay applied to the viewport(s) until you remove them.
        */
        void* _dccNode = nullptr;
    };
}//end of namespace

#endif //FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_H
