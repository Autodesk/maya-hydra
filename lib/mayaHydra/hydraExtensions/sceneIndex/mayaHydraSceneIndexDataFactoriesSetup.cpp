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

/* This class creates the scene index data factories and set them up into the flow viewport library to be able to create DCC 
*   specific scene index data classes without knowing their content in Flow viewport.
*   This is done in the constructor of this class
*/

//Local headers
#include "mayaHydraSceneIndexDataFactoriesSetup.h"
#include "mayaHydraMayaDataProducerSceneIndexDataConcreteFactory.h"
#include "mayaHydraMayaFilteringSceneIndexDataConcreteFactory.h"

//Flow viewport headers
#include <flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h>
#include <flowViewport/API/interfacesImp/fvpFilteringSceneIndexInterfaceImp.h>

namespace MAYAHYDRA_NS_DEF{

SceneIndexDataFactoriesSetup::SceneIndexDataFactoriesSetup()
{
    static MayaFilteringSceneIndexDataConcreteFactory  filteringFactory;
    Fvp::FilteringSceneIndexInterfaceImp::get().setSceneIndexDataFactory(filteringFactory);

    static MayaDataProducerSceneIndexDataConcreteFactory  dataProducerFactory;
    Fvp::DataProducerSceneIndexInterfaceImp::get().setSceneIndexDataFactory(dataProducerFactory);
}

}//end of namespace MAYAHYDRA_NS_DEF 