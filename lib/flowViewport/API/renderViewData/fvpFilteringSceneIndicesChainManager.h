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

#ifndef FLOW_VIEWPORT_API_RENDERVIEWDATA_FILTERING_SCENE_INDEX_CHAIN_MANAGER
#define FLOW_VIEWPORT_API_RENDERVIEWDATA_FILTERING_SCENE_INDEX_CHAIN_MANAGER

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

//Std headers
#include <string>

namespace FVP_NS_DEF {

class RenderViewData;//Predeclaration

/**Is a singleton to manage the custom filtering scene indices chain which is appended after the merging scene index
* To access this class, use Fvp::FilteringSceneIndicesChainManager& filteringSceneIndicesChainManager = Fvp::FilteringSceneIndicesChainManager::get();
*/
class FVP_API FilteringSceneIndicesChainManager
{
public:
    /// Destructor
    ~FilteringSceneIndicesChainManager();

    ///Singleton accessor
    static FilteringSceneIndicesChainManager& get();

    /**  
    *   @brief  Create the filtering scene indices chain for this render view. 
    *   @param[in] viewData is the RenderViewData from the hydra render view.
    *   @param[in] inputFilteringSceneIndex is the input scene index for your filtering scene index.
    *   @return the latest scene index from the custom filtering scene indices chain
    */
    PXR_NS::HdSceneIndexBaseRefPtr  createFilteringSceneIndicesChain(RenderViewData& viewData,
                                                                    const PXR_NS::HdSceneIndexBaseRefPtr& inputFilteringSceneIndex = nullptr);

    /**  
    *   @brief  Removes from the render index the last element of the filtering scene indices chain for this render view and delete the whole chain
    *   @param[in] viewData is the RenderViewData from the hydra render view.
    */
    void destroyFilteringSceneIndicesChain(RenderViewData& viewData);

    /**
    *  @brief       Update the whole filtering scene indices chains.
    *
    *               Update the whole chain by destroying it then create it again (use case is : a new FilteringSceneIndexClient was registered / unregistered 
    *               so we must re-create the filtering scene indices chain with this change.
    *               We update only the render views whose renderer display name is in rendererDisplayName.
    * 
    *  @param[in]   rendererDisplayNames is a string containing either nothing ("") meaning this should apply to all renderers 
    *               or it contains one or more renderers display names such as ("GL, Arnold") and in this case we must update 
    *               only the render views filtering scene indices chain which are using this renderer.
    */
    void updateFilteringSceneIndicesChain(const std::string& rendererDisplayNames);

    // For debugging purpose : enable/disable the filtering scene indices chain as a global switch.
    void setEnabled(bool enabled);
    bool getEnabled()const {return _enabled;}

private:
    /**
    *   @brief  Create the filtering scene indices chain for this render view. 
    *   @param[in] viewData is the RenderViewData from the hydra render view.
    *   @param[in] inputFilteringSceneIndex is the input scene index for your filtering scene index.
    */
    void _AppendFilteringSceneIndicesChain( RenderViewData& viewData, 
                                            const PXR_NS::HdSceneIndexBaseRefPtr& inputScene);

    /// Private constructor
    FilteringSceneIndicesChainManager() = default;

    ///Enable/Disable the Filtering scene indices chain for debugging purpose
    bool _enabled {true};
};


}//End of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_API_RENDERVIEWDATA_FILTERING_SCENE_INDEX_CHAIN_MANAGER

