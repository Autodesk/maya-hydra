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
#ifndef MAYA_HYDRA_HYDRA_EXTENSIONS_SCENE_INDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_CONCRETE_FACTORY_H
#define MAYA_HYDRA_HYDRA_EXTENSIONS_SCENE_INDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_CONCRETE_FACTORY_H

//MayaHydra headers
#include "mayaHydraLib/api.h"
#include "mayaHydraLib/mayaHydra.h"

//Flow Viewport headers
#include <flowViewport/API/perViewportSceneIndicesData/fvpDataProducerSceneIndexDataAbstractFactory.h>

namespace MAYAHYDRA_NS_DEF {

/** Since Flow viewport is DCC agnostic, the DCC implements a concrete factory subclassing that class to provide specific DCC implementation of the classes mentioned.
* */
class MayaDataProducerSceneIndexDataConcreteFactory : public Fvp::DataProducerSceneIndexDataAbstractFactory
{
public:
    /// CreateDataProducerSceneIndexDataBase creates a subclass of DataProducerSceneIndexDataBaseRefPtr with specific DCC variables that flow viewport cannot manage since it's DCC agnostic
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr   
        createDataProducerSceneIndexDataBase(const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params) override;

    /** The DCC will create a subclass of DataProducerSceneIndexDataBaseRefPtr with specific DCC variables that Flow viewport cannot manage since it's DCC agnostic
    * This function is specific for Usd Stages
    */
    PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr   createDataProducerSceneIndexDataBaseForUsdStage(
        PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params)override;
};

}//end of namespace MAYAHYDRA_NS_DEF 

#endif //MAYA_HYDRA_HYDRA_EXTENSIONS_SCENE_INDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_CONCRETE_FACTORY_H

