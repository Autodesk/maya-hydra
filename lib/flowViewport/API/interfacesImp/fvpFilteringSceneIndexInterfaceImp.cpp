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
#include "fvpFilteringSceneIndexInterfaceImp.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h"

//Std headers
#include <mutex>

namespace
{
    std::mutex selectionHighlightFilteringClient_mutex;
    std::mutex sceneFilteringClient_mutex;

    //Set of scene Filtering scene index data, they belong to the Fpv::FilteringSceneIndexClient::Category::kSceneFiltering
    std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>& sceneFilteringSceneIndicesData() {
        static std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>
#ifdef CODE_COVERAGE_WORKAROUND
            *data{nullptr};
        if (!data) {
            data = new std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>;
        }
        return *data;
#else
            data;
        return data;
#endif
    }

    //Set of selection highlighting Filtering scene index data, they belong to the Fpv::FilteringSceneIndexClient::Category::kSelectionHighlighting
    std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr> selectionHighlightFilteringSceneIndicesData;

    // Abstract factory to create the scene index data, an implementation is provided by the DCC
    FVP_NS::FilteringSceneIndexDataAbstractFactory* sceneIndexDataFactory{nullptr};
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

FilteringSceneIndexInterface& FilteringSceneIndexInterface::get() 
{ 
    return FilteringSceneIndexInterfaceImp::get();
}

FilteringSceneIndexInterfaceImp& FilteringSceneIndexInterfaceImp::get()
{
    static FilteringSceneIndexInterfaceImp theInterface;
    return theInterface;
}

FilteringSceneIndexInterfaceImp::~FilteringSceneIndexInterfaceImp()
{
}

bool FilteringSceneIndexInterfaceImp::registerFilteringSceneIndexClient(const std::shared_ptr<FilteringSceneIndexClient>& client)
{
    switch(client->getCategory()){
        case Fvp::FilteringSceneIndexClient::Category::kSceneFiltering:{
            return _CreateSceneFilteringSceneIndicesData(client);
        }
        break;
        case Fvp::FilteringSceneIndexClient::Category::kSelectionHighlighting:{
            return _CreateSelectionHighlightFilteringSceneIndicesData(client);
        }
        break;
        default:{
            TF_AXIOM(0);//Should never happen
        }
    }
    return false;
}

bool FilteringSceneIndexInterfaceImp::_CreateSceneFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client) 
{ 
    TF_AXIOM(sceneIndexDataFactory);

    bool bNeedToUpdateViewportsFilteringSceneIndicesChain = false;

    //Block for the lock lifetime
    {
        std::lock_guard<std::mutex> lock(sceneFilteringClient_mutex);

        auto findResult = std::find_if(sceneFilteringSceneIndicesData().cbegin(), sceneFilteringSceneIndicesData().cend(),
                    [&client](const PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr& filteringSIData) { return filteringSIData->GetClient() == client;});
        if (!TF_VERIFY(findResult == sceneFilteringSceneIndicesData().cend(),
                       "Filtering scene index client already found in FilteringSceneIndexInterfaceImp::_CreateSceneFilteringSceneIndicesData()")){
            return false;
        }

        //Call the abstract scene index data factory to create a subclass of FilteringSceneIndexDataBase
        PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr data = sceneIndexDataFactory->createFilteringSceneIndexDataBase(client);
        sceneFilteringSceneIndicesData().insert(data);
        bNeedToUpdateViewportsFilteringSceneIndicesChain = true;
    }

    if (bNeedToUpdateViewportsFilteringSceneIndicesChain){
        FilteringSceneIndicesChainManager::get().updateFilteringSceneIndicesChain(client->getRendererNames());
    }

    return true;
}

bool FilteringSceneIndexInterfaceImp::_CreateSelectionHighlightFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client)
{ 
    TF_AXIOM(sceneIndexDataFactory);

    //Block for the lock lifetime
    {
        std::lock_guard<std::mutex> lock(selectionHighlightFilteringClient_mutex);

        auto findResult = std::find_if(selectionHighlightFilteringSceneIndicesData.cbegin(), selectionHighlightFilteringSceneIndicesData.cend(),
                    [&client](const PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr& filteringSIData) { return filteringSIData->GetClient() == client;});
        if (findResult != selectionHighlightFilteringSceneIndicesData.cend()){
            return false;
        }

        //Call the abstract scene index data factory to create a subclass of FilteringSceneIndexDataBase
        PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr data = sceneIndexDataFactory->createFilteringSceneIndexDataBase(client);
        selectionHighlightFilteringSceneIndicesData.insert(data);
    }

    //TODO add it somewhere in selection highlight

    return true;
}

void FilteringSceneIndexInterfaceImp::_DestroySceneFilteringSceneIndicesData(const std::shared_ptr<::FVP_NS_DEF::FilteringSceneIndexClient>& client)
{
    
    bool bNeedToUpdateViewportsFilteringSceneIndicesChain = false;
    std::string rendererNames;

    //Block for the lock lifetime
    {
        std::lock_guard<std::mutex> lock(sceneFilteringClient_mutex);

        auto findResult = std::find_if(sceneFilteringSceneIndicesData().cbegin(), sceneFilteringSceneIndicesData().cend(),
                    [&client](const PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr& filteringSIData) { return filteringSIData->GetClient() == client;});
        if (findResult != sceneFilteringSceneIndicesData().cend()){
            const auto& filteringSIData = (*findResult);
            rendererNames = (filteringSIData)
                ? filteringSIData->GetClient()->getRendererNames()
                : FvpViewportAPITokens->allRenderers;

            sceneFilteringSceneIndicesData().erase(findResult);//This also decreases ref count

            bNeedToUpdateViewportsFilteringSceneIndicesChain = true;
        }
    }

    if (bNeedToUpdateViewportsFilteringSceneIndicesChain){
        //Update the filtering scene indices chain from the viewports that were using this filtering scene index client
        FilteringSceneIndicesChainManager::get().updateFilteringSceneIndicesChain(rendererNames);
    }
}

void FilteringSceneIndexInterfaceImp::_DestroySelectionHighlightFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client)
{
    //Block for the lock lifetime
    {
        std::lock_guard<std::mutex> lock(selectionHighlightFilteringClient_mutex);

        auto findResult = std::find_if(selectionHighlightFilteringSceneIndicesData.cbegin(), selectionHighlightFilteringSceneIndicesData.cend(),
                    [&client](const PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr& filteringSIData) { return filteringSIData->GetClient() == client;});
        if (findResult != selectionHighlightFilteringSceneIndicesData.cend()){
            selectionHighlightFilteringSceneIndicesData.erase(findResult);//Also decreases ref count
        }
    }
}

void FilteringSceneIndexInterfaceImp::unregisterFilteringSceneIndexClient(const std::shared_ptr<FilteringSceneIndexClient>& client)
{
    switch(client->getCategory()){
        case Fvp::FilteringSceneIndexClient::Category::kSceneFiltering:{
            _DestroySceneFilteringSceneIndicesData(client);
        }
        break;
        case Fvp::FilteringSceneIndexClient::Category::kSelectionHighlighting:{
            _DestroySelectionHighlightFilteringSceneIndicesData(client);
        }
        break;
        default:{
            TF_AXIOM(0);//Should never happen
        }
    }
}

const std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>& FilteringSceneIndexInterfaceImp::getSceneFilteringSceneIndicesData()const
{
    std::lock_guard<std::mutex> lock(sceneFilteringClient_mutex);
    return sceneFilteringSceneIndicesData();
}

const std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>& FilteringSceneIndexInterfaceImp::getSelectionHighlightFilteringSceneIndicesData() const
{
    std::lock_guard<std::mutex> lock(selectionHighlightFilteringClient_mutex);
    return selectionHighlightFilteringSceneIndicesData;
}

void FilteringSceneIndexInterfaceImp::setSceneIndexDataFactory(FilteringSceneIndexDataAbstractFactory& factory) 
{
    sceneIndexDataFactory = &factory;
}

}//End of namespace FVP_NS_DEF

