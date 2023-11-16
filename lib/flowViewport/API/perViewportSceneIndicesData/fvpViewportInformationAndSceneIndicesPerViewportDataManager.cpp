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
#include "flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>

//STL Headers
#include <mutex>

namespace 
{
    std::mutex _viewportInformationAndSceneIndicesPerViewportDataSet_mutex;
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

ViewportInformationAndSceneIndicesPerViewportDataManager& ViewportInformationAndSceneIndicesPerViewportDataManager::Get()
{
    static ViewportInformationAndSceneIndicesPerViewportDataManager theViewportInformationAndSceneIndicesPerViewportDataManager;
    return theViewportInformationAndSceneIndicesPerViewportDataManager;
}

//A new Hydra viewport was created
void ViewportInformationAndSceneIndicesPerViewportDataManager::AddViewportInformation(const InformationInterface::ViewportInformation& viewportInfo, Fvp::RenderIndexProxy& renderIndexProxy)
{
    //Add it in our array if it is not already inside
    {
        std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

        auto findResult = std::find_if(_viewportInformationAndSceneIndicesPerViewportDataSet.cbegin(), _viewportInformationAndSceneIndicesPerViewportDataSet.cend(),
                    [&viewportInfo](const ViewportInformationAndSceneIndicesPerViewportData& other) { 
                        return other.GetViewportInformation() == viewportInfo;
                    }
        );
        if (findResult == _viewportInformationAndSceneIndicesPerViewportDataSet.cend()){
            const ViewportInformationAndSceneIndicesPerViewportData::CreationParameters creationParams(viewportInfo, renderIndexProxy);
            _viewportInformationAndSceneIndicesPerViewportDataSet.insert(ViewportInformationAndSceneIndicesPerViewportData(creationParams));
        }else{
            return; //It is already inside our array
        }
    }

    //Let the registered clients know a new viewport has been added
    InformationInterfaceImp::Get().SceneIndexAdded(viewportInfo);
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveViewportInformation(const HdSceneIndexBaseRefPtr& viewportSceneIndex)
{
    //Block for the lifetime of the lock
    {
        std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);
    
        auto findResult = std::find_if(_viewportInformationAndSceneIndicesPerViewportDataSet.begin(), _viewportInformationAndSceneIndicesPerViewportDataSet.end(),
                    [&viewportSceneIndex](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportSceneIndex == viewportSceneIndex;});
        if (findResult != _viewportInformationAndSceneIndicesPerViewportDataSet.end()){

            InformationInterfaceImp::Get().SceneIndexRemoved(findResult->GetViewportInformation());

            const RenderIndexProxy& renderIndexProxy = findResult->GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

            //Destroy the custom filtering scene indices chain
            //Following code is equivalent to calling FilteringSceneIndicesChainManager::GetManager().DestroyFilteringSceneIndicesChain(*renderIndexProxy);
            //But we cannot do so because of the lock above.
            auto renderIndex = renderIndexProxy.GetRenderIndex();
            if (renderIndex && findResult->GetLastFilteringSceneIndexOfTheChain()){
                renderIndex->RemoveSceneIndex(findResult->GetLastFilteringSceneIndexOfTheChain());//Remove the whole chain from the render index
            }
            
            _viewportInformationAndSceneIndicesPerViewportDataSet.erase(findResult);
        }
    }
}

const InformationInterface::ViewportInformation* 
ViewportInformationAndSceneIndicesPerViewportDataManager::GetViewportInformationFromViewportSceneIndex(const HdSceneIndexBaseRefPtr& viewportSceneIndex) const
{
    std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    auto findResult = std::find_if(_viewportInformationAndSceneIndicesPerViewportDataSet.cbegin(), _viewportInformationAndSceneIndicesPerViewportDataSet.cend(),
                [&viewportSceneIndex](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportSceneIndex == viewportSceneIndex;});
    if (findResult != _viewportInformationAndSceneIndicesPerViewportDataSet.cend()){
        return &(findResult->GetViewportInformation());
    }

    return nullptr;
}

const RenderIndexProxy* 
ViewportInformationAndSceneIndicesPerViewportDataManager::GetRenderIndexProxyFromViewportSceneIndex(const HdSceneIndexBaseRefPtr& viewportSceneIndex) const
{
    std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    auto findResult = std::find_if(_viewportInformationAndSceneIndicesPerViewportDataSet.cbegin(), _viewportInformationAndSceneIndicesPerViewportDataSet.cend(),
                [&viewportSceneIndex](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportSceneIndex == viewportSceneIndex;});
    if (findResult != _viewportInformationAndSceneIndicesPerViewportDataSet.cend()){
        return &(findResult->GetRenderIndexProxy());
    }

    return nullptr;
}

const ViewportInformationAndSceneIndicesPerViewportData* ViewportInformationAndSceneIndicesPerViewportDataManager::
    GetViewportInformationAndSceneIndicesPerViewportDataFromViewportSceneIndex(const HdSceneIndexBaseRefPtr& viewportSceneIndex) const
{
    std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

    auto findResult = std::find_if(_viewportInformationAndSceneIndicesPerViewportDataSet.cbegin(), _viewportInformationAndSceneIndicesPerViewportDataSet.cend(),
                [&viewportSceneIndex](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportSceneIndex == viewportSceneIndex;});
    if (findResult != _viewportInformationAndSceneIndicesPerViewportDataSet.cend()){
        return &(*findResult);
    }

    return nullptr;
}

} //End of namespace FVP_NS_DEF {

