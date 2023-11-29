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

//Local headers
#include "fvpFilteringSceneIndicesChainManager.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"
#include "flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>
#include <pxr/imaging/hd/renderIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

//Singleton access
FilteringSceneIndicesChainManager& FilteringSceneIndicesChainManager::get()
{ 
    static FilteringSceneIndicesChainManager FilteringSceneIndicesChainManager;
    return FilteringSceneIndicesChainManager;
}

FilteringSceneIndicesChainManager::~FilteringSceneIndicesChainManager()
{
}

HdSceneIndexBaseRefPtr FilteringSceneIndicesChainManager::createFilteringSceneIndicesChain(ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData,
                                                                                        const HdSceneIndexBaseRefPtr& inputFilteringSceneIndex /*= nullptr*/)
{
    HdSceneIndexBaseRefPtr inputSceneIndex = inputFilteringSceneIndex;
    if (nullptr == inputSceneIndex){
        inputSceneIndex = viewportInformationAndSceneIndicesPerViewportData.GetInputSceneIndex();
    }else{
        viewportInformationAndSceneIndicesPerViewportData.SetInputSceneIndex(inputSceneIndex);
    }
    TF_AXIOM(inputSceneIndex);

    if ( ! _enabled){
        return inputSceneIndex;
    }
     
    if (viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain() != nullptr){
        TF_CODING_ERROR("viewportInformationAndSceneIndicesPerViewportData->GetLastFilteringSceneIndexOfTheChain() != nullptr should not happen, \
            you should call DestroyFilteringSceneIndicesChain before calling the current  function");
        return nullptr;//Not an empty filtering scene indices chain
    }

    //Append the filtering scene indices chain tp the merging scene index from renderIndexProxy
    _AppendFilteringSceneIndicesChain(viewportInformationAndSceneIndicesPerViewportData, inputSceneIndex);
    
    if (viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain() == nullptr){
        TF_CODING_ERROR("viewportInformationAndSceneIndicesPerViewportData->GetLastFilteringSceneIndexOfTheChain() == nullptr is invalid here");
        return nullptr;
    }

    //Add the last element of the filtering scene indices chain to the render index
    return viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain();
}

void FilteringSceneIndicesChainManager::destroyFilteringSceneIndicesChain(ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData)
{
    HdSceneIndexBaseRefPtr& lastSceneIndexOfTheChain = viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain();
    if (nullptr == lastSceneIndexOfTheChain){
        return;
    }

    auto renderIndexProxy = viewportInformationAndSceneIndicesPerViewportData.GetRenderIndexProxy();
    TF_AXIOM(renderIndexProxy);
    auto renderIndex = renderIndexProxy->GetRenderIndex();
    TF_AXIOM(renderIndex);
    renderIndex->RemoveSceneIndex(lastSceneIndexOfTheChain);//Remove the whole chain from the render index

    //Remove a ref on it which should cascade the same on its references
    lastSceneIndexOfTheChain.Reset();
}

void FilteringSceneIndicesChainManager::updateFilteringSceneIndicesChain(const std::string& rendererDisplayNames)
{
    /*  rendererDisplayName is a string containing either FvpViewportAPITokens->allRenderers meaning this should apply to all renderers 
    *   or it contains one or more renderers display names such as ("GL, Arnold") and in this case we must update 
    *   only the viewports filtering scene indices chain which are using this renderer.
    */

    ViewportInformationAndSceneIndicesPerViewportDataSet& viewportInformationAndSceneIndicesPerViewportDataSet=
            ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetAllViewportInfoAndData();

    for (auto& viewportInformationAndSceneIndicesPerViewportData : viewportInformationAndSceneIndicesPerViewportDataSet){
        
        //Check the renderer name
        const std::string& rendererDisplayName = viewportInformationAndSceneIndicesPerViewportData.GetViewportInformation()._rendererName;
        if ( (FvpViewportAPITokens->allRenderers != rendererDisplayNames) && (! rendererDisplayName.empty()) ){
            //Filtering per renderer is applied
            if (std::string::npos == rendererDisplayNames.find(rendererDisplayName)){
                continue; //Ignore this filtering scene indices chain since the renderer is different
            }
        }

        ViewportInformationAndSceneIndicesPerViewportData& nonConstViewportInformationAndSceneIndicesPerViewportData = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(viewportInformationAndSceneIndicesPerViewportData);
        auto& renderIndexProxy = viewportInformationAndSceneIndicesPerViewportData.GetRenderIndexProxy();
        destroyFilteringSceneIndicesChain(nonConstViewportInformationAndSceneIndicesPerViewportData);
        createFilteringSceneIndicesChain(nonConstViewportInformationAndSceneIndicesPerViewportData);
        auto& lastSceneIndex = viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain();
        TF_AXIOM(lastSceneIndex && renderIndexProxy && renderIndexProxy->GetRenderIndex());
        renderIndexProxy->GetRenderIndex()->InsertSceneIndex(lastSceneIndex, SdfPath::AbsoluteRootPath());
    }
}

void FilteringSceneIndicesChainManager::_AppendFilteringSceneIndicesChain(  ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData, 
                                                                            const HdSceneIndexBaseRefPtr& inputScene)
{
    TF_AXIOM(inputScene);

    HdContainerDataSourceHandle _inputArgs = nullptr;//Possibility to send custom data for scene index registration
        
    const std::string& rendererDisplayName = viewportInformationAndSceneIndicesPerViewportData.GetViewportInformation()._rendererName;
    
    HdSceneIndexBaseRefPtr& lastSceneIndexOfTheChain    = viewportInformationAndSceneIndicesPerViewportData.GetLastFilteringSceneIndexOfTheChain();
    //Set the merging scene index as the last element to use this scene index as the input scene index of filtering scene indices
    lastSceneIndexOfTheChain                            = inputScene;

    //Call our Hydra viewport API mechanism for custom filtering scene index clients
    auto& viewportFilteringSceneIndicesData = FilteringSceneIndexInterfaceImp::get().getSceneFilteringSceneIndicesData(); //Not const as we may modify its content later
    for (auto& filteringSceneIndexData : viewportFilteringSceneIndicesData) {
        auto& client = filteringSceneIndexData->getClient();
        const std::string& rendererNames = client.getRendererNames();
        //Filter by render delegate name
        if ( (FvpViewportAPITokens->allRenderers != rendererNames) && rendererNames.find(rendererDisplayName) == std::string::npos){
            //Ignore that client info, it is not targeted for this renderer
            continue;
        }

        const bool isVisible = filteringSceneIndexData->getVisible();
        if (! isVisible){
            continue; //We should not happened the filtering scene indices from not visible ViewportFilteringSceneIndexData
        }

        auto tempAppendedSceneIndex = client.appendSceneIndex(lastSceneIndexOfTheChain, _inputArgs);
        if ((lastSceneIndexOfTheChain != tempAppendedSceneIndex)){
            //A new scene index was appended, it can also be a chain of scene indices but we need only the last element
            lastSceneIndexOfTheChain = tempAppendedSceneIndex;
        }
    }
}

void FilteringSceneIndicesChainManager::setEnabled(bool enable)
{
    if (_enabled != enable){
        _enabled = enable;
    }

    //Update all viewports
    updateFilteringSceneIndicesChain(FvpViewportAPITokens->allRenderers);
}

}//End of namespace FVP_NS_DEF