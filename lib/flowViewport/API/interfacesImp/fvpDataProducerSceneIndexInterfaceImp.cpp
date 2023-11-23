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
#include "flowViewport/API/perViewportSceneIndicesData/fvpViewportInformationAndSceneIndicesPerViewportDataManager.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>
 
//STL Headers
#include <mutex>

namespace
{
    std::mutex _dataProducerSceneIndicesThatApplyToAllViewports_mutex;

    // Are the scene indices that need to be applied to all viewports
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr> _dataProducerSceneIndicesThatApplyToAllViewports;

    // Abstract factory to create the scene index data, an implementation is provided by the DCC
    FVP_NS::DataProducerSceneIndexDataAbstractFactory* _sceneIndexDataFactory{nullptr};
}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

const std::string DataProducerSceneIndexInterface::allViewports  = "allViewports";
const std::string DataProducerSceneIndexInterface::allRenderers  = "allRenderers";

DataProducerSceneIndexInterface& DataProducerSceneIndexInterface::get()
{
    return DataProducerSceneIndexInterfaceImp::get();
}

DataProducerSceneIndexInterfaceImp& DataProducerSceneIndexInterfaceImp::get()
{
    static DataProducerSceneIndexInterfaceImp theInterface;
    return theInterface;
}

void DataProducerSceneIndexInterfaceImp::addDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                   void* dccNode /*= nullptr*/,
                                                                   const std::string& hydraViewportId /*= allViewports*/,
                                                                   const std::string& rendererNames /*= allRenderers*/,
                                                                   const PXR_NS::SdfPath& customDataProducerSceneIndexRootPathForInsertion /*= PXR_NS::SdfPath::AbsoluteRootPath()*/)
{   
    //_viewportSceneIndex can be a reference on a nullptr meaning the user wants _customDataProducerSceneIndex to be applied in all viewports.
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexData  = 
        _CreateDataProducerSceneIndexData(customDataProducerSceneIndex, rendererNames, customDataProducerSceneIndexRootPathForInsertion, dccNode);

    if (DataProducerSceneIndexInterface::allViewports == hydraViewportId){
        //Apply this dataProducer scene index to all viewports
        _AddDataProducerSceneIndexToAllViewports(dataProducerSceneIndexData);
    } else{
        //Apply this dataProducer scene index to a single viewport
        const ViewportInformationAndSceneIndicesPerViewportData* viewportInfoAndData = 
            ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetViewportInfoAndDataFromViewportId(hydraViewportId);
        if (viewportInfoAndData){
            _AddDataProducerSceneIndexToThisViewport(viewportInfoAndData->GetViewportInformation(), dataProducerSceneIndexData);
        }
    }
}

void DataProducerSceneIndexInterfaceImp::removeAllViewportDataProducerSceneIndices(ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData)
{
    auto& renderIndexProxy = viewportInformationAndSceneIndicesPerViewportData.GetRenderIndexProxy();
    if(nullptr == renderIndexProxy){
        return;
    }

    auto& dataProducerSceneIndicesDataForthisViewport = viewportInformationAndSceneIndicesPerViewportData.GetDataProducerSceneIndicesData();

    for (auto& dataProducerSceneIndicesData : dataProducerSceneIndicesDataForthisViewport){
        //Remove it from the render index
        if (dataProducerSceneIndicesData){
            const auto& sceneIndex = dataProducerSceneIndicesData->GetDataProducerLastSceneIndexChain();
            if (sceneIndex){
                renderIndexProxy->RemoveSceneIndex(sceneIndex);
            }else{
                TF_CODING_ERROR("dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain() is a nullptr, that should never happen here.");
            }
        }
    }

    dataProducerSceneIndicesDataForthisViewport.clear();
}

void DataProducerSceneIndexInterfaceImp::removeViewportDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                              const std::string& hydraViewportId /*= allViewports*/)
{
    if (DataProducerSceneIndexInterface::allViewports == hydraViewportId){
        //It was applied to all viewports
        
        ViewportInformationAndSceneIndicesPerViewportDataSet& allViewportsInfoAndSceneIndices = 
            ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetAllViewportInfoAndData();

        //We need to remove it from all viewports where it was applied.
        for (auto& viewportInfoAndData : allViewportsInfoAndSceneIndices){
            ViewportInformationAndSceneIndicesPerViewportData& nonConstViewportInfoAndData = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(viewportInfoAndData);
            nonConstViewportInfoAndData.RemoveViewportDataProducerSceneIndex(customDataProducerSceneIndex);
        }

        //Also remove it from the _dataProducerSceneIndicesThatApplyToAllViewports array
        auto findResult = std::find_if(_dataProducerSceneIndicesThatApplyToAllViewports.begin(), _dataProducerSceneIndicesThatApplyToAllViewports.end(),
                    [&customDataProducerSceneIndex](const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSIData) { 
                        return (dataProducerSIData && dataProducerSIData->GetDataProducerSceneIndex() == customDataProducerSceneIndex);}
        );
        if (findResult != _dataProducerSceneIndicesThatApplyToAllViewports.end()){
            _dataProducerSceneIndicesThatApplyToAllViewports.erase(findResult);// Which also decreases ref count
        }
    }else{
        //It was applied to a single viewport
        auto viewportInformationAndSceneIndicesPerViewportData = ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetViewportInfoAndDataFromViewportId(hydraViewportId);
        viewportInformationAndSceneIndicesPerViewportData->RemoveViewportDataProducerSceneIndex(customDataProducerSceneIndex);
    }
}

void DataProducerSceneIndexInterfaceImp::_AddDataProducerSceneIndexToAllViewports(const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData)
{ 
    //Remove const from _dataProducerSceneIndexData
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr dataProducerSceneIndexDataNonConst = dataProducerSceneIndexData;
    
    //This is a block for the mutex lifetime
    {
        std::lock_guard<std::mutex> lockDataProducerSceneIndicesDataPerViewport(_dataProducerSceneIndicesThatApplyToAllViewports_mutex);
        
        //Check if it is already inside our array
        auto findResult = _dataProducerSceneIndicesThatApplyToAllViewports.find(dataProducerSceneIndexDataNonConst);
        if (findResult != _dataProducerSceneIndicesThatApplyToAllViewports.cend()){
            return;
        }

        //It is not already in dataProducerSceneIndexDataSet
        //Add it with the dataProducer scene indices that need to be applied to all viewports
        _dataProducerSceneIndicesThatApplyToAllViewports.insert(dataProducerSceneIndexDataNonConst);
    }

    //Apply it to all existing hydra viewports
    InformationInterface::ViewportInformationSet viewportsInformation;
    InformationInterfaceImp::Get().GetViewportsInformation(viewportsInformation);
    for (const auto& viewportInfo : viewportsInformation){
        _AddDataProducerSceneIndexToThisViewport(viewportInfo, dataProducerSceneIndexData);
    }
}

PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr DataProducerSceneIndexInterfaceImp::_CreateDataProducerSceneIndexData(const HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                                             const std::string& rendererNames,
                                                                                             const SdfPath& customDataProducerSceneIndexRootPathForInsertion, 
                                                                                             void* dccNode)
{ 
    TF_AXIOM(_sceneIndexDataFactory);

    if (! _sceneIndexDataFactory){
        TF_CODING_ERROR("_sceneIndexDataFactory is a nullptr, it should have been provided by a call to GetDataProducerSceneIndexInterfaceImp()->SetSceneIndexDataFactory");
        return nullptr;
    }

    const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters  params(customDataProducerSceneIndex, rendererNames, customDataProducerSceneIndexRootPathForInsertion, dccNode);
    return _sceneIndexDataFactory->createDataProducerSceneIndexDataBase(params);
}

void DataProducerSceneIndexInterfaceImp::_AddDataProducerSceneIndexToThisViewport(const InformationInterface::ViewportInformation& viewportInformation, 
                                                                                  const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData)
{
    TF_AXIOM(dataProducerSceneIndexData);

    const std::string& hydraViewportId = viewportInformation._viewportId;
    TF_AXIOM(hydraViewportId.length() > 0);

    //Check if there is some filtering per Hydra renderer
    const std::string& viewportRendererName                         = viewportInformation._rendererName;
    const std::string& dataProducerSceneIndexApplyToRendererNames   = dataProducerSceneIndexData->GetRendererNames();
    if ( (! viewportRendererName.empty() )&& (dataProducerSceneIndexApplyToRendererNames != DataProducerSceneIndexInterface::allRenderers) ){
        //Filtering per renderer is applied
        if (std::string::npos == dataProducerSceneIndexApplyToRendererNames.find(viewportRendererName)){
            return; //Ignore the current hydra viewport renderer name is not part of the supported renderers for this dataProducer scene index
        }
    }

    ViewportInformationAndSceneIndicesPerViewportData* viewportInformationAndSceneIndicesPerViewportData = 
        ViewportInformationAndSceneIndicesPerViewportDataManager::Get().GetViewportInfoAndDataFromViewportId(hydraViewportId);
    TF_AXIOM(viewportInformationAndSceneIndicesPerViewportData );
    
    auto& dataProducerSceneIndicesDataForthisViewport = viewportInformationAndSceneIndicesPerViewportData->GetDataProducerSceneIndicesData();
    auto findResult = dataProducerSceneIndicesDataForthisViewport.find(dataProducerSceneIndexData);
    if (findResult != dataProducerSceneIndicesDataForthisViewport.end()){
        return; //Already in our array
    }
    
    dataProducerSceneIndicesDataForthisViewport.insert(dataProducerSceneIndexData);//dataProducerSceneIndexData can be shared between multiple viewports
    
    //Add it to the merging scene index if the render inex proxy is present, it may happen that it will be set later
    auto renderIndexProxy = viewportInformationAndSceneIndicesPerViewportData->GetRenderIndexProxy();
    if (renderIndexProxy && dataProducerSceneIndexData && dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain()){
        renderIndexProxy->InsertSceneIndex(dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain(), dataProducerSceneIndexData->GetCustomDataProducerSceneIndexRootPathForInsertion());
    }
}

void DataProducerSceneIndexInterfaceImp::hydraViewportSceneIndexAdded(const InformationInterface::ViewportInformation& viewportInfo)
{
    //Add the dataProducer scene indices that apply to all viewports to this newly created hydra viewport
    std::lock_guard<std::mutex> lockDataProducerSceneIndicesDataPerViewport(_dataProducerSceneIndicesThatApplyToAllViewports_mutex);
    for (const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData : _dataProducerSceneIndicesThatApplyToAllViewports){
        _AddDataProducerSceneIndexToThisViewport(viewportInfo, dataProducerSceneIndexData);
    }
}

void DataProducerSceneIndexInterfaceImp::setSceneIndexDataFactory(DataProducerSceneIndexDataAbstractFactory& factory) 
{
    _sceneIndexDataFactory = &factory;
}

} //End of namespace FVP_NS_DEF
