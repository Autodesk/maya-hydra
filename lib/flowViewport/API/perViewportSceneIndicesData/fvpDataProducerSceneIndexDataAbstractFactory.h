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
#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATAPRODUCERSCENEINDEXDATAABSTRACTFACTORY_H
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATAPRODUCERSCENEINDEXDATAABSTRACTFACTORY_H

//Flow Viewport headers
#include "fvpDataProducerSceneIndexDataBase.h"

namespace FVP_NS_DEF {

/** Since Flow viewport is DCC agnostic, the DCC will implement a concrete factory subclassing that class to provide a specific 
*   DCC implementation of DataProducerSceneIndexDataBaseRefPtr.
*/
class DataProducerSceneIndexDataAbstractFactory
{
public:
    /// The DCC will create a subclass of DataProducerSceneIndexDataBaseRefPtr with specific DCC variables that Flow viewport cannot manage since it's DCC agnostic
    virtual PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr   createDataProducerSceneIndexDataBase(
        const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params) = 0;

    /** The DCC will create a subclass of DataProducerSceneIndexDataBaseRefPtr with specific DCC variables that Flow viewport cannot manage since it's DCC agnostic
    * This function is specific for Usd Stages
    */
    virtual PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr   createDataProducerSceneIndexDataBaseForUsdStage(
        PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params) = 0;
    
};

}//End of namespace FVP_NS_DEF {

#endif //FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATAPRODUCERSCENEINDEXDATAABSTRACTFACTORY_H

