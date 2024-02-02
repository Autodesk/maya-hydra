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



#ifndef FLOW_VIEWPORT_API_INTERFACESIMP_DATA_PRODUCER_SCENE_INDEX_INTERFACE_IMP_H
#define FLOW_VIEWPORT_API_INTERFACESIMP_DATA_PRODUCER_SCENE_INDEX_INTERFACE_IMP_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpDataProducerSceneIndexInterface.h"
#include "flowViewport/API/fvpInformationInterface.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpDataProducerSceneIndexDataAbstractFactory.h"
#include "flowViewport/API/fvpViewportAPITokens.h"

//Std Headers
#include <set>

//Hydra headers
#include <pxr/usdImaging/usdImaging/sceneIndices.h>


namespace FVP_NS_DEF {

class ViewportInformationAndSceneIndicesPerViewportData;

///Is a singleton, use Fvp::DataProducerSceneIndexInterfaceImp& dataProducerSceneIndexInterfaceImp = Fvp::DataProducerSceneIndexInterfaceImp::Get() to get an instance of that interface
class DataProducerSceneIndexInterfaceImp : public DataProducerSceneIndexInterface
{
public:
    DataProducerSceneIndexInterfaceImp()             = default;
    virtual ~DataProducerSceneIndexInterfaceImp()    = default;

    ///Interface accessor
    static FVP_API DataProducerSceneIndexInterfaceImp& get();

    ///Specific internal function for Usd Stages
    FVP_API PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr addUsdStageSceneIndex( PXR_NS::UsdImagingCreateSceneIndicesInfo& createInfo,
                                        PXR_NS::HdSceneIndexBaseRefPtr& finalSceneIndex,
                                        PXR_NS::UsdImagingStageSceneIndexRefPtr& stageSceneIndex,
                                        PXR_NS::SdfPath& inoutPreFix,
                                        void* dccNode);

    ///Specific internal function for Usd Stages
    FVP_API bool addUsdStageDataProducerSceneIndexDataBaseToAllViewports(PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr&  dataProducerSceneIndexData);

    ///From FVP_NS_DEF::DataProducerSceneIndexInterface
    bool addDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                    PXR_NS::SdfPath& inoutPreFix,
                                    void* dccNode = nullptr,
                                    const std::string& hydraViewportId = PXR_NS::FvpViewportAPITokens->allViewports,
                                    const std::string& rendererNames = PXR_NS::FvpViewportAPITokens->allRenderers
                                    )override;
    void removeViewportDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                              const std::string& hydraViewportId = PXR_NS::FvpViewportAPITokens->allViewports)override;


    FVP_API void ClearDataProducerSceneIndicesThatApplyToAllViewports();

    //Called by flow viewport
    ///hydraViewportSceneIndexAdded is called when a new hydra viewport is created by the ViewportInformationAndSceneIndicesPerViewportDataManager, it's not a callback.
    void hydraViewportSceneIndexAdded(const InformationInterface::ViewportInformation& viewportInfo);
    void removeAllViewportDataProducerSceneIndices(ViewportInformationAndSceneIndicesPerViewportData& viewportInformationAndSceneIndicesPerViewportData);

    ///Since Flow viewport is DCC agnostic, the DCC will implement a concrete factory and call setSceneIndexDataFactory to register it in this class.
    FVP_API
    void setSceneIndexDataFactory(DataProducerSceneIndexDataAbstractFactory& factory);

protected:
    bool _AddDataProducerSceneIndexToAllViewports(const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData);
    void _AddDataProducerSceneIndexToThisViewport(const InformationInterface::ViewportInformation& viewportInformation, const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData);
    
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr _CreateDataProducerSceneIndexData( const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex,
                                                                const std::string& rendererNames,
                                                                const PXR_NS::SdfPath& prefix, 
                                                                void* dccNode);
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr _CreateDataProducerSceneIndexDataForUsdStage(PXR_NS::UsdImagingCreateSceneIndicesInfo& createInfo, 
                                                                                                          PXR_NS::HdSceneIndexBaseRefPtr& finalSceneIndex, 
                                                                                                          PXR_NS::UsdImagingStageSceneIndexRefPtr& stageSceneIndex,
                                                                                                              const PXR_NS::SdfPath& inoutPreFix, 
                                                                                                              void* dccNode);
};

} //End of namespace FVP_NS_DEF


#endif // FLOW_VIEWPORT_API_INTERFACESIMP_DATA_PRODUCER_SCENE_INDEX_INTERFACE_IMP_H