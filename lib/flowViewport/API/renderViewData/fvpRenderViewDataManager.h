//
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef FLOW_VIEWPORT_API_RENDERVIEWDATA_RENDER_VIEW_DATA_MANAGER_H
#define FLOW_VIEWPORT_API_RENDERVIEWDATA_RENDER_VIEW_DATA_MANAGER_H

//Local headers
#include "fvpRenderViewData.h"
#include "flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF {

/** Is a singleton to manage the RenderViewData which stores information and misc. scene indices data per render view
*   So, if there are "n" Hydra views in the DCC, we will have "n" instances of RenderViewData.
* 
*   To get an instance of this class, please use 
*   RenderViewDataManager& manager = RenderViewDataManager:Get();
*/
class FVP_API RenderViewDataManager
{
public:
    /// Manager accessor
    static RenderViewDataManager& Get();
 
    //A new Hydra view was created, we need inputSceneIndexForCustomFiltering to be used as an input scene index for custom filtering scene indices
    //return true if some data producer scene indices were added
    bool AddRenderViewData(const InformationInterface::RenderViewDesc& viewDesc, 
                           PXR_NS::HdRenderIndex* renderIndex, 
                           const Fvp::DataProducerMergingSceneIndexProxyPtr& dataProducerMergingSceneIndexProxy, 
                           const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndexForCustomFiltering);
    
    //A Hydra view was deleted
    void RemoveRenderViewData(const std::string& viewId);

    const RenderViewDataVector&  GetAllViewData() const {return _renderViewsData;}
    RenderViewDataVector&  GetAllViewData() {return _renderViewsData;}

    const RenderViewData* GetViewDataFromViewId(const std::string& viewId)const;
    RenderViewData* GetViewDataFromViewId(const std::string& viewId);

    const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>&  GetDataProducerSceneIndicesDataFromViewId(const std::string& viewId)const;

    bool ViewIsAlreadyRegistered(const std::string& viewId)const;
    void RemoveAllRenderViewData();

private:

    // Singleton, no public creation or copy.
    RenderViewDataManager() = default;
    RenderViewDataManager(
        const RenderViewDataManager&
    ) = delete;

    ///Hydra views data
    RenderViewDataVector     _renderViewsData;
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_RENDERVIEWDATA_RENDER_VIEW_DATA_MANAGER_H
