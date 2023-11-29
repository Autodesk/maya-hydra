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

//Local headers
#include "fvpViewportInformationAndSceneIndicesPerViewportDataManager.h"
#include "flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h"
#include "flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>

//STL Headers
#include <mutex>

namespace 
{
    std::mutex viewportInformationAndSceneIndicesPerViewportDataSet_mutex;
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr> dummyEmptyArray;
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

ViewportInformationAndSceneIndicesPerViewportDataManager& ViewportInformationAndSceneIndicesPerViewportDataManager::Get()
{
    static ViewportInformationAndSceneIndicesPerViewportDataManager theViewportInformationAndSceneIndicesPerViewportDataManager;
    return theViewportInformationAndSceneIndicesPerViewportDataManager;
}

//A new Hydra viewport was created
void ViewportInformationAndSceneIndicesPerViewportDataManager::AddViewportInformation(const InformationInterface::ViewportInformation& viewportInfo, const Fvp::RenderIndexProxyPtr& renderIndexProxy, 
                                                                    const HdSceneIndexBaseRefPtr& lastFilteringSceneIndexOfTheChainBeforeCustomFiltering)
{
    TF_AXIOM(renderIndexProxy && lastFilteringSceneIndexOfTheChainBeforeCustomFiltering);

    ViewportInformationAndSceneIndicesPerViewportDataSet::iterator it = _viewportsInformationAndSceneIndicesPerViewportData.end();

    //Add it in our array if it is not already inside
    {
        std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

        const auto& viewportId = viewportInfo._viewportId;
        auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
        if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){
            return;//It is already inside our array
        }

        ViewportInformationAndSceneIndicesPerViewportData temp(viewportInfo, renderIndexProxy);
        auto theResultingPair = _viewportsInformationAndSceneIndicesPerViewportData.emplace(temp);
        it = theResultingPair.first;
    }

    //Call this to let the data producer scene indices that apply to all viewports to be added to this new viewport as well
    DataProducerSceneIndexInterfaceImp::get().hydraViewportSceneIndexAdded(viewportInfo);

    //Let the registered clients know a new viewport has been added
    InformationInterfaceImp::Get().SceneIndexAdded(viewportInfo);

    //Add the custom filtering scene indices to the merging scene index
    ViewportInformationAndSceneIndicesPerViewportData& viewportsInformationAndSceneIndicesPerViewportData = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(*it);
    const HdSceneIndexBaseRefPtr lastFilteringSceneIndex  = FilteringSceneIndicesChainManager::get().createFilteringSceneIndicesChain(viewportsInformationAndSceneIndicesPerViewportData, 
                                                                                                                                lastFilteringSceneIndexOfTheChainBeforeCustomFiltering);

    //Insert the last filtering scene index into the render index
    auto renderIndex = renderIndexProxy->GetRenderIndex();
    TF_AXIOM(renderIndex);
    renderIndex->InsertSceneIndex(lastFilteringSceneIndex, SdfPath::AbsoluteRootPath());
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveViewportInformation(const std::string& modelPanel)
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);
    
    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){

        InformationInterfaceImp::Get().SceneIndexRemoved(findResult->GetViewportInformation());

        const Fvp::RenderIndexProxyPtr& renderIndexProxy = findResult->GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

        if(renderIndexProxy){
            //Destroy the custom filtering scene indices chain
            auto renderIndex = renderIndexProxy->GetRenderIndex();
            const auto& filteringSceneIndex = findResult->GetLastFilteringSceneIndexOfTheChain();
            if (renderIndex && filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
            
        _viewportsInformationAndSceneIndicesPerViewportData.erase(findResult);
    }
}

const ViewportInformationAndSceneIndicesPerViewportData* ViewportInformationAndSceneIndicesPerViewportDataManager::GetViewportInfoAndDataFromViewportId(const std::string& viewportId)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.cbegin(), _viewportsInformationAndSceneIndicesPerViewportData.cend(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.cend()){
        const ViewportInformationAndSceneIndicesPerViewportData& data = (*findResult);
        return &data;
    }

    return nullptr;
}

ViewportInformationAndSceneIndicesPerViewportData* ViewportInformationAndSceneIndicesPerViewportDataManager::GetViewportInfoAndDataFromViewportId(const std::string& viewportId)
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    ViewportInformationAndSceneIndicesPerViewportDataSet::iterator findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){
        ViewportInformationAndSceneIndicesPerViewportData& data = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(*findResult);
        return &data;
    }

    return nullptr;
}

const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>&  
ViewportInformationAndSceneIndicesPerViewportDataManager::GetDataProducerSceneIndicesDataFromViewportId(const std::string& viewportId)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    for (const auto& viewportInformationAndSceneIndicesPerViewportData : _viewportsInformationAndSceneIndicesPerViewportData){
        const auto& viewportIdFromContainer   = viewportInformationAndSceneIndicesPerViewportData.GetViewportInformation()._viewportId;
        if (viewportIdFromContainer == viewportId){
            return viewportInformationAndSceneIndicesPerViewportData.GetDataProducerSceneIndicesData();
        }
    }

    return dummyEmptyArray;
}

bool ViewportInformationAndSceneIndicesPerViewportDataManager::ModelPanelIsAlreadyRegistered(const std::string& modelPanel)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.cbegin(), _viewportsInformationAndSceneIndicesPerViewportData.cend(),
                    [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});

    return (findResult != _viewportsInformationAndSceneIndicesPerViewportData.cend());
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveAllViewportsInformation()
{ 
    //Block for the lifetime of the lock
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportDataSet_mutex);
    
    for(auto& viewportInfoAndData :_viewportsInformationAndSceneIndicesPerViewportData){

        InformationInterfaceImp::Get().SceneIndexRemoved(viewportInfoAndData.GetViewportInformation());

        const Fvp::RenderIndexProxyPtr& renderIndexProxy = viewportInfoAndData.GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

        if(renderIndexProxy){
            //Destroy the custom filtering scene indices chain
            auto renderIndex = renderIndexProxy->GetRenderIndex();
            const auto& filteringSceneIndex = viewportInfoAndData.GetLastFilteringSceneIndexOfTheChain();
            if (renderIndex && filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
    }

    _viewportsInformationAndSceneIndicesPerViewportData.clear();//Delete all of them
}

} //End of namespace FVP_NS_DEF {

