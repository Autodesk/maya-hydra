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
#include "fvpDataProducerSceneIndexInterfaceImp.h"
#include "fvpInformationInterfaceImp.h"
#include "flowViewport/API/renderViewData/fvpRenderViewDataManager.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>
 
//Std Headers
#include <mutex>

/// DataProducersNodeHashCodeToSdfPathRegistry does a mapping between DCC nodes hash code and Hydra
/// paths. The DCC nodes registered in this class are used by data producers scene indices as a
/// parent to all primitives. The registration/unregistration in this class is automatic when you
/// use the flow viewport API and provide a DCC node as a parent. This class is used when we select
/// one of these nodes to return the matching SdfPath so that all child prims of this node are
/// highlighted.
/// 
namespace
{
    std::mutex dataProducerSceneIndicesThatApplyToAllViews_mutex;

    // Are the scene indices that need to be applied to all render views
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr> dataProducerSceneIndicesThatApplyToAllViews;

    // Abstract factory to create the scene index data, an implementation is provided by the DCC
    FVP_NS::DataProducerSceneIndexDataAbstractFactory* sceneIndexDataFactory{nullptr};
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

DataProducerSceneIndexInterface& DataProducerSceneIndexInterface::get()
{
    return DataProducerSceneIndexInterfaceImp::get();
}

DataProducerSceneIndexInterfaceImp& DataProducerSceneIndexInterfaceImp::get()
{
    static DataProducerSceneIndexInterfaceImp theInterface;
    return theInterface;
}

//Specific internal function for Usd Stages
PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr DataProducerSceneIndexInterfaceImp::addUsdStageSceneIndex(UsdImagingCreateSceneIndicesInfo& createInfo, 
                                                                                HdSceneIndexBaseRefPtr& finalSceneIndex,
                                                                                UsdImagingStageSceneIndexRefPtr& stageSceneIndex,
                                                                                const SdfPath& preFix, 
                                                                                void* dccNode)
{
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
        _CreateDataProducerSceneIndexDataForUsdStage(createInfo, finalSceneIndex, stageSceneIndex, preFix, dccNode);
    if (nullptr == dataProducerSceneIndexData){
        return nullptr;
    }

    return dataProducerSceneIndexData; 
}

bool DataProducerSceneIndexInterfaceImp::addUsdStageDataProducerSceneIndexDataBaseToAllViews(PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr&  dataProducerSceneIndexData){
    //Apply this usd scene index to all render views
    return _AddDataProducerSceneIndexToAllViews(dataProducerSceneIndexData);
}

bool DataProducerSceneIndexInterfaceImp::addDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                   const PXR_NS::SdfPath& preFix,
                                                                   void* dccNode /*= nullptr*/,
                                                                   const std::string& viewId /*= allRenderViews*/,
                                                                   const std::string& rendererNames /*= allRenderers*/
                                                                    )
{   
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
        _CreateDataProducerSceneIndexData(customDataProducerSceneIndex, rendererNames, preFix, dccNode);
    if (nullptr == dataProducerSceneIndexData){
        return false;
    }

    //PXR_NS::FvpViewportAPITokens->allRenderViews == viewId means the user wants customDataProducerSceneIndex to be applied in all render views.
    if (PXR_NS::FvpViewportAPITokens->allRenderViews == viewId){
        //Apply this data producer scene index to all render views
        return _AddDataProducerSceneIndexToAllViews(dataProducerSceneIndexData);
    } 

    //Apply this data producer scene index to a single render view
    const RenderViewData* viewData = 
        RenderViewDataManager::Get().GetViewDataFromViewId(viewId);
    if (viewData){
        _AddDataProducerSceneIndexToThisView(viewData->GetViewDesc(), dataProducerSceneIndexData);
        return true;
    }

    return false;
}

void DataProducerSceneIndexInterfaceImp::removeAllDataProducerSceneIndicesFromView(RenderViewData& viewData)
{
    auto dataProducerMergingSceneIndexProxy = viewData.GetDataProducerMergingSceneIndexProxy();
    if (nullptr == dataProducerMergingSceneIndexProxy) {
        return;
    }

    auto& dataProducerSceneIndicesDataForThisView = viewData.GetDataProducerSceneIndicesData();

    for (const auto& dataProducerSceneIndicesData : dataProducerSceneIndicesDataForThisView){
        // Remove the data producer scene index from the merging scene index
        if (dataProducerSceneIndicesData){
            const auto& sceneIndex = dataProducerSceneIndicesData->GetDataProducerLastSceneIndexChain();
            if (sceneIndex){
                dataProducerMergingSceneIndexProxy->RemoveSceneIndex(sceneIndex);
            }else{
                TF_CODING_ERROR("dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain() is a nullptr, that should never happen here.");
            }
        }
    }

    dataProducerSceneIndicesDataForThisView.clear();
}

void DataProducerSceneIndexInterfaceImp::removeDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                      const std::string& viewId /*= allRenderViews*/)
{
    if (PXR_NS::FvpViewportAPITokens->allRenderViews == viewId){
        //It was applied to all render views
        
        RenderViewDataVector& allViewData = 
            RenderViewDataManager::Get().GetAllViewData();

        //We need to remove it from all render views where it was applied.
        for (auto& viewData : allViewData){
            viewData.RemoveDataProducerSceneIndex(customDataProducerSceneIndex);
        }

        //Also remove it from the dataProducerSceneIndicesThatApplyToAllViews array
        auto findResult = std::find_if(dataProducerSceneIndicesThatApplyToAllViews.begin(), dataProducerSceneIndicesThatApplyToAllViews.end(),
                    [&customDataProducerSceneIndex](const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSIData) { 
                        return (dataProducerSIData && dataProducerSIData->GetDataProducerSceneIndex() == customDataProducerSceneIndex);}
        );
        if (findResult != dataProducerSceneIndicesThatApplyToAllViews.end()){
            dataProducerSceneIndicesThatApplyToAllViews.erase(findResult);// Which also decreases ref count
        }
    }else{
        //It was applied to a single render view
        auto viewData = RenderViewDataManager::Get().GetViewDataFromViewId(viewId);
        viewData->RemoveDataProducerSceneIndex(customDataProducerSceneIndex);
    }
}

bool DataProducerSceneIndexInterfaceImp::_AddDataProducerSceneIndexToAllViews(const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData)
{ 
    //Remove const from _dataProducerSceneIndexData
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexDataNonConst = dataProducerSceneIndexData;
    
    //This is a block for the mutex lifetime
    {
        std::lock_guard<std::mutex> lockDataProducerSceneIndicesDataPerView(dataProducerSceneIndicesThatApplyToAllViews_mutex);
        
        //Check if it is already inside our array
        auto findResult = dataProducerSceneIndicesThatApplyToAllViews.find(dataProducerSceneIndexDataNonConst);
        if (findResult != dataProducerSceneIndicesThatApplyToAllViews.cend()){
            return false;
        }

        //It is not already in dataProducerSceneIndexDataSet
        //Add it with the data producer scene indices that need to be applied to all render views
        dataProducerSceneIndicesThatApplyToAllViews.insert(dataProducerSceneIndexDataNonConst);
    }

    //Apply it to all existing hydra render views
    InformationInterface::RenderViewDescSet viewDescs;
    InformationInterfaceImp::Get().GetAllRenderViewDescs(viewDescs);
    for (const auto& viewDesc : viewDescs){
        _AddDataProducerSceneIndexToThisView(viewDesc, dataProducerSceneIndexData);
    }

    return true;
}

PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr DataProducerSceneIndexInterfaceImp::_CreateDataProducerSceneIndexDataForUsdStage(
    PXR_NS::UsdImagingCreateSceneIndicesInfo& createInfo, HdSceneIndexBaseRefPtr& finalSceneIndex, UsdImagingStageSceneIndexRefPtr& stageSceneIndex, const SdfPath& prefix, void* dccNode)
{ 
    TF_AXIOM(sceneIndexDataFactory);

    if (! sceneIndexDataFactory){
        TF_CODING_ERROR("sceneIndexDataFactory is a nullptr, it should have been provided by a call to GetDataProducerSceneIndexInterfaceImp()->SetSceneIndexDataFactory");
        return nullptr;
    }

    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage  params(createInfo, finalSceneIndex, stageSceneIndex, prefix, dccNode);
    return sceneIndexDataFactory->createDataProducerSceneIndexDataBaseForUsdStage(params);
}


PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr DataProducerSceneIndexInterfaceImp::_CreateDataProducerSceneIndexData(const HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                                             const std::string& rendererNames,
                                                                                             const SdfPath& prefix, 
                                                                                             void* dccNode)
{ 
    TF_AXIOM(sceneIndexDataFactory);

    if (! sceneIndexDataFactory){
        TF_CODING_ERROR("sceneIndexDataFactory is a nullptr, it should have been provided by a call to GetDataProducerSceneIndexInterfaceImp()->SetSceneIndexDataFactory");
        return nullptr;
    }

    const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters  params(customDataProducerSceneIndex, rendererNames, prefix, dccNode);
    return sceneIndexDataFactory->createDataProducerSceneIndexDataBase(params);
}

void DataProducerSceneIndexInterfaceImp::_AddDataProducerSceneIndexToThisView(const InformationInterface::RenderViewDesc& viewDesc, 
                                                                                  const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData)
{
    TF_AXIOM(dataProducerSceneIndexData);

    const std::string& viewId = viewDesc._viewId;
    TF_AXIOM(viewId.length() > 0);
    
    //Check if there is some filtering per Hydra renderer
    const std::string& viewRendererName                         = viewDesc._rendererName;
    const std::string& dataProducerSceneIndexApplyToRendererNames   = dataProducerSceneIndexData->GetRendererNames();
    if ( (! viewRendererName.empty() )&& (dataProducerSceneIndexApplyToRendererNames != PXR_NS::FvpViewportAPITokens->allRenderers) ){
        //Filtering per renderer is applied
        if (std::string::npos == dataProducerSceneIndexApplyToRendererNames.find(viewRendererName)){
            return; //Ignore the current hydra render view renderer name is not part of the supported renderers for this data producer scene index
        }
    }

    RenderViewData* viewData = 
        RenderViewDataManager::Get().GetViewDataFromViewId(viewId);
    TF_AXIOM(viewData );
    
    auto& dataProducerSceneIndicesDataForthisView = viewData->GetDataProducerSceneIndicesData();
    auto findResult = dataProducerSceneIndicesDataForthisView.find(dataProducerSceneIndexData);
    if (findResult != dataProducerSceneIndicesDataForthisView.end()){
        return; //Already in our array
    }
    
    dataProducerSceneIndicesDataForthisView.insert(dataProducerSceneIndexData);//dataProducerSceneIndexData can be shared between multiple render views
    
    //Add it to the merging scene index if the merging scene index is present, it may happen that it will be set later
    auto dataProducerMergingSceneIndexProxy = viewData->GetDataProducerMergingSceneIndexProxy();
    if (dataProducerMergingSceneIndexProxy && dataProducerSceneIndexData && dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain()){
        dataProducerMergingSceneIndexProxy->InsertSceneIndex(dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain(), dataProducerSceneIndexData->GetPrefix());
    }
}

bool DataProducerSceneIndexInterfaceImp::hydraViewSceneIndexAdded(const InformationInterface::RenderViewDesc& viewDesc)
{
    bool dataProducerSceneIndicesAdded = false;
    //Add the data producer scene indices that apply to all render views to this newly created hydra render view
    std::lock_guard<std::mutex> lockDataProducerSceneIndicesDataPerView(dataProducerSceneIndicesThatApplyToAllViews_mutex);
    for (const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData : dataProducerSceneIndicesThatApplyToAllViews){
        _AddDataProducerSceneIndexToThisView(viewDesc, dataProducerSceneIndexData);
        dataProducerSceneIndicesAdded = true;
    }

    return dataProducerSceneIndicesAdded;
}

void DataProducerSceneIndexInterfaceImp::setSceneIndexDataFactory(DataProducerSceneIndexDataAbstractFactory& factory) 
{
    sceneIndexDataFactory = &factory;
}

void DataProducerSceneIndexInterfaceImp::ClearDataProducerSceneIndicesThatApplyToAllViews() 
{ 
    std::lock_guard<std::mutex> lockDataProducerSceneIndicesDataPerView(dataProducerSceneIndicesThatApplyToAllViews_mutex);
    dataProducerSceneIndicesThatApplyToAllViews.clear();
}

} //End of namespace FVP_NS_DEF
