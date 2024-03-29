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
#include "mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.h"
#include "mayaHydraMayaDataProducerSceneIndexData.h"

namespace MAYAHYDRA_NS_DEF{

PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr 
MayaDataProducerSceneIndexDataConcreteFactory::createDataProducerSceneIndexDataBase(const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params)
{
    return PXR_NS::MayaDataProducerSceneIndexData::New(params);
}

PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr   
MayaDataProducerSceneIndexDataConcreteFactory::createDataProducerSceneIndexDataBaseForUsdStage(PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params)
{
    return PXR_NS::MayaDataProducerSceneIndexData::New(params);
}

}//end of namespace MAYAHYDRA_NS_DEF 