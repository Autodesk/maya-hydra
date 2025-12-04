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
#include "fvpInformationInterfaceImp.h"
#include "flowViewport/API/renderViewData/fvpRenderViewDataManager.h"

#include <mutex>

namespace{
    std::mutex informationClient_mutex;
    
    //Set of information clients
    FVP_NS_DEF::SharedInformationClientPtrSet informationClients;
}
    
PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {


InformationInterface& InformationInterface::Get() 
{ 
    return InformationInterfaceImp::Get();
}

InformationInterfaceImp& InformationInterfaceImp::Get()
{
    static InformationInterfaceImp theInterface;
    return theInterface;
}

void InformationInterfaceImp::RegisterInformationClient(const std::shared_ptr<InformationClient>& client)
{
    TF_AXIOM(client);

    std::lock_guard<std::mutex> lock(informationClient_mutex);

    auto foundResult = informationClients.find(client);
    if (foundResult == informationClients.cend()){
        informationClients.insert(client);
    }
}

void InformationInterfaceImp::UnregisterInformationClient(const std::shared_ptr<InformationClient>& client)
{
    std::lock_guard<std::mutex> lock(informationClient_mutex);

    auto foundResult = informationClients.find(client);
    if (foundResult != informationClients.end()){
        informationClients.erase(foundResult);
    }
}

void InformationInterfaceImp::SceneIndexAdded(const InformationInterface::RenderViewDesc& viewDesc)
{
    std::lock_guard<std::mutex> lock(informationClient_mutex);
    for (auto infoClient : informationClients){
        if (infoClient){
            infoClient->SceneIndexAdded(viewDesc);
        }
    }
}

void InformationInterfaceImp::SceneIndexRemoved(const InformationInterface::RenderViewDesc& viewDesc)
{
    std::lock_guard<std::mutex> lock(informationClient_mutex);
    for (auto infoClient : informationClients){
        if (infoClient){
            infoClient->SceneIndexRemoved(viewDesc);
        }
    }
}

void InformationInterfaceImp::GetAllRenderViewDescs(RenderViewDescSet& outRenderViewDescs)const
{
    outRenderViewDescs.clear();
    const RenderViewDataVector& allViewData = RenderViewDataManager::Get().GetAllViewData();
    for (const RenderViewData& viewData : allViewData){
        const InformationInterface::RenderViewDesc& viewDesc = viewData.GetViewDesc();
        outRenderViewDescs.insert(viewDesc);
    }
}

} //End of namespace FVP_NS_DEF {

