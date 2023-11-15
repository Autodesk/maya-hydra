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
#include "flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h"

#include <mutex>

namespace{
    std::mutex _viewportInformationClient_mutex;
    
    //Set of information clients
    FVP_NS_DEF::InformationClientSet _viewportInformationClients;
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

void InformationInterfaceImp::RegisterInformationClient(const InformationClient& client)
{
    std::lock_guard<std::mutex> lock(_viewportInformationClient_mutex);

    InformationClient* clientNonConst = const_cast<InformationClient*>(&client);
    auto foundResult = _viewportInformationClients.find(clientNonConst);
    if (foundResult == _viewportInformationClients.cend()){
        _viewportInformationClients.insert(clientNonConst);
    }
}

void InformationInterfaceImp::UnregisterInformationClient(const InformationClient& client)
{
    std::lock_guard<std::mutex> lock(_viewportInformationClient_mutex);

    InformationClient* clientNonConst = const_cast<InformationClient*>(&client);
    auto foundResult = _viewportInformationClients.find(clientNonConst);
    if (foundResult != _viewportInformationClients.end()){
        _viewportInformationClients.erase(foundResult);
    }
}

void InformationInterfaceImp::SceneIndexAdded(const InformationInterface::ViewportInformation& _viewportInfo)
{
    std::lock_guard<std::mutex> lock(_viewportInformationClient_mutex);
    for (auto viewportInfoClient : _viewportInformationClients){
        if (viewportInfoClient){
            viewportInfoClient->SceneIndexAdded(_viewportInfo);
        }
    }
}

void InformationInterfaceImp::SceneIndexRemoved(const InformationInterface::ViewportInformation& _viewportInfo)
{
    std::lock_guard<std::mutex> lock(_viewportInformationClient_mutex);
    for (auto viewportInfoClient : _viewportInformationClients){
        if (viewportInfoClient){
            viewportInfoClient->SceneIndexRemoved(_viewportInfo);
        }
    }
}

void InformationInterfaceImp::GetViewportsInformation(std::set<const ViewportInformation*>& outHydraViewportInformationArray)const
{
    outHydraViewportInformationArray.clear();
    const ViewportInformationAndSceneIndicesPerViewportDataSet& viewportInformationAndSceneIndicesPerViewportDataArray = 
        ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetViewportInformationAndSceneIndicesPerViewportDataSet();
    for (const ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData : viewportInformationAndSceneIndicesPerViewportDataArray){
        const InformationInterface::ViewportInformation& viewportInfo = viewportInformationAndSceneIndicesPerViewportData.GetViewportInformation();
        outHydraViewportInformationArray.insert(&viewportInfo);
    }
}

} //End of namespace FVP_NS_DEF {

