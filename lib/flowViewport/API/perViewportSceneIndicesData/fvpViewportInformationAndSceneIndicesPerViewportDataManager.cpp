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
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"

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
void ViewportInformationAndSceneIndicesPerViewportDataManager::AddViewportInformation(const InformationInterface::ViewportInformation& viewportInfo)
{
    //Add it in our array if it is not already inside
    {
        std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);

        auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.cbegin(), _viewportsInformationAndSceneIndicesPerViewportData.cend(),
                    [&viewportInfo](const ViewportInformationAndSceneIndicesPerViewportData& other) { 
                        return other.GetViewportInformation() == viewportInfo;
                    }
        );

        if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.cend()){
            return;//It is already inside our array
        }

        _viewportsInformationAndSceneIndicesPerViewportData.insert(ViewportInformationAndSceneIndicesPerViewportData(viewportInfo));
    }

    //Let the registered clients know a new viewport has been added
    InformationInterfaceImp::Get().SceneIndexAdded(viewportInfo);
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveViewportInformation(const std::string& modelPanel)
{
    //Block for the lifetime of the lock
    {
        std::lock_guard<std::mutex> lock(_viewportInformationAndSceneIndicesPerViewportDataSet_mutex);
    
        auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});
        if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){

            InformationInterfaceImp::Get().SceneIndexRemoved(findResult->GetViewportInformation());

            const std::shared_ptr<Fvp::RenderIndexProxy> renderIndexProxy = findResult->GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

            if(renderIndexProxy){
                //Destroy the custom filtering scene indices chain
                //Following code is equivalent to calling FilteringSceneIndicesChainManager::GetManager().DestroyFilteringSceneIndicesChain(*renderIndexProxy);
                //But we cannot do so because of the lock above.
                auto renderIndex = renderIndexProxy->GetRenderIndex();
                if (renderIndex && findResult->GetLastFilteringSceneIndexOfTheChain()){
                    renderIndex->RemoveSceneIndex(findResult->GetLastFilteringSceneIndexOfTheChain());//Remove the whole chain from the render index
                }
            }
            
            _viewportsInformationAndSceneIndicesPerViewportData.erase(findResult);
        }
    }
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::UpdateRenderIndexProxy(const std::string& modelPanel, const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy)
{
    if (! renderIndexProxy){
        return;
    }

    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){
        if(findResult->GetRenderIndexProxy()){
            return; //Already updated
        }

        auto& item = *findResult;
        ViewportInformationAndSceneIndicesPerViewportData& nonConstItem = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(item);
        nonConstItem.SetRenderIndexProxy(renderIndexProxy);
    }
}

} //End of namespace FVP_NS_DEF {

