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
#include "fvpRenderViewDataManager.h"
#include "flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h"
#include "flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h"
#include "flowViewport/API/renderViewData/fvpFilteringSceneIndicesChainManager.h"
#include "flowViewport/API/renderViewData/fvpIsolateSelectManager.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>

//Std Headers
#include <mutex>

namespace 
{
    std::mutex renderViewsData_mutex;
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr> dummyEmptyArray;

#ifdef CODE_COVERAGE_WORKAROUND
void leakViewData(const Fvp::RenderViewDataVector& dataVec) {
    // Must place the leaked data vector on the heap, as a by-value
    // vector will have its destructor called at process exit, which calls
    // the vector element destructors and triggers the crash. 
    static std::vector<Fvp::RenderViewDataVector>* leakedData{nullptr};
    if (!leakedData) {
        leakedData = new std::vector<Fvp::RenderViewDataVector>;
    }
    leakedData->push_back(dataVec);
}
#endif

}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

RenderViewDataManager& RenderViewDataManager::Get()
{
    static RenderViewDataManager instance;
    return instance;
}

//A new Hydra view was created
bool RenderViewDataManager::AddRenderViewData(
    const InformationInterface::RenderViewDesc& viewDesc, 
    PXR_NS::HdRenderIndex* renderIndex,
    const Fvp::DataProducerMergingSceneIndexProxyPtr& dataProducerMergingSceneIndexProxy,
    const HdSceneIndexBaseRefPtr& inputSceneIndexForCustomFiltering)
{
    TF_AXIOM(renderIndex && dataProducerMergingSceneIndexProxy && inputSceneIndexForCustomFiltering);

    RenderViewData* newElement = nullptr;

    //Add it in our array if it is not already inside
    {
        std::lock_guard<std::mutex> lock(renderViewsData_mutex);

        const auto& viewId = viewDesc._viewId;
        auto findResult = std::find_if(_renderViewsData.begin(), _renderViewsData.end(),
                    [&viewId](const RenderViewData& other) { return other.GetViewDesc()._viewId == viewId;});
        if (findResult != _renderViewsData.end()){
            return false;//It is already inside our array
        }

        RenderViewData temp(viewDesc, renderIndex, dataProducerMergingSceneIndexProxy);
        newElement = &(_renderViewsData.emplace_back(temp));
    }

    //Call this to let the data producer scene indices that apply to all views to be added to this new view as well
    const bool dataProducerSceneIndicesAdded = DataProducerSceneIndexInterfaceImp::get().hydraViewSceneIndexAdded(viewDesc);

    //Let the registered clients know a new view has been added
    InformationInterfaceImp::Get().SceneIndexAdded(viewDesc);

    //Add the custom filtering scene indices to the merging scene index
    TF_AXIOM(newElement);
    const HdSceneIndexBaseRefPtr lastFilteringSceneIndex  = FilteringSceneIndicesChainManager::get().createFilteringSceneIndicesChain(*newElement, 
                                                                                                                                inputSceneIndexForCustomFiltering);
    //Insert the last filtering scene index into the render index
    renderIndex->InsertSceneIndex(lastFilteringSceneIndex, SdfPath::AbsoluteRootPath());

    return dataProducerSceneIndicesAdded;
}

void RenderViewDataManager::RemoveRenderViewData(const std::string& viewId)
{
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);
    
    auto findResult = std::find_if(_renderViewsData.begin(), _renderViewsData.end(),
                [&viewId](const RenderViewData& other) { return other.GetViewDesc()._viewId == viewId;});
    if (findResult != _renderViewsData.end()){

        InformationInterfaceImp::Get().SceneIndexRemoved(findResult->GetViewDesc());

        auto renderIndex = findResult->GetRenderIndex();//Get the pointer on the renderIndex

        if (renderIndex) {
            //Destroy the custom filtering scene indices chain
            const auto& filteringSceneIndex = findResult->GetLastFilteringSceneIndex();
            if (filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
            
        _renderViewsData.erase(findResult);
    }
}

const RenderViewData* RenderViewDataManager::GetViewDataFromViewId(const std::string& viewId)const
{
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);

    auto findResult = std::find_if(_renderViewsData.cbegin(), _renderViewsData.cend(),
                    [&viewId](const RenderViewData& other) { return other.GetViewDesc()._viewId == viewId;});
    if (findResult != _renderViewsData.cend()){
        const RenderViewData& data = (*findResult);
        return &data;
    }

    return nullptr;
}

RenderViewData* RenderViewDataManager::GetViewDataFromViewId(const std::string& viewId)
{
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);

    RenderViewDataVector::iterator findResult = std::find_if(_renderViewsData.begin(), _renderViewsData.end(),
                    [&viewId](const RenderViewData& other) { return other.GetViewDesc()._viewId == viewId;});
    if (findResult != _renderViewsData.end()){
        RenderViewData& data = const_cast<RenderViewData&>(*findResult);
        return &data;
    }

    return nullptr;
}


const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>&  
RenderViewDataManager::GetDataProducerSceneIndicesDataFromViewId(const std::string& viewId)const
{
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);

    for (const auto& viewData : _renderViewsData){
        const auto& viewIdFromContainer   = viewData.GetViewDesc()._viewId;
        if (viewIdFromContainer == viewId){
            return viewData.GetDataProducerSceneIndicesData();
        }
    }

    return dummyEmptyArray;
}

bool RenderViewDataManager::ViewIsAlreadyRegistered(const std::string& viewId)const
{
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);

    auto findResult = std::find_if(_renderViewsData.cbegin(), _renderViewsData.cend(),
                    [&viewId](const RenderViewData& other) { return other.GetViewDesc()._viewId == viewId;});

    return (findResult != _renderViewsData.cend());
}

void RenderViewDataManager::RemoveAllRenderViewData()
{ 
    //Block for the lifetime of the lock
    std::lock_guard<std::mutex> lock(renderViewsData_mutex);
    
    for(auto& viewData :_renderViewsData){

        InformationInterfaceImp::Get().SceneIndexRemoved(viewData.GetViewDesc());

        auto renderIndex = viewData.GetRenderIndex();//Get the pointer on the renderIndex

        if (renderIndex) {
            //Destroy the custom filtering scene indices chain
            const auto& filteringSceneIndex = viewData.GetLastFilteringSceneIndex();
            if (filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
    }

#ifdef CODE_COVERAGE_WORKAROUND
    leakViewData(_renderViewsData);
#endif

    _renderViewsData.clear();//Delete all of them

    IsolateSelectManager::Get().Reset();
}

} //End of namespace FVP_NS_DEF {

