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
#ifndef MAYA_HYDRA_FILTERING_SCENE_INDEX_DATA_CONCRETE_FACTORY_H
#define MAYA_HYDRA_FILTERING_SCENE_INDEX_DATA_CONCRETE_FACTORY_H

//MayaHydra headers
#include "mayaHydraLib/api.h"
#include "mayaHydraLib/mayaHydra.h"

//Flow Viewport headers
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndexDataAbstractFactory.h"

namespace MAYAHYDRA_NS_DEF {

/** Since Flow viewport is DCC agnostic, the DCC will implement a concrete factory subclassing that class to provide specific DCC implementation of FilteringSceneIndexDataBaseRefPtr.
*/
class MayaFilteringSceneIndexDataConcreteFactory : public Fvp::FilteringSceneIndexDataAbstractFactory
{
public:
    /// The DCC will create a subclass of FilteringSceneIndexDataBaseRefPtr with specific DCC variables that Flow viewport cannot manage since it's DCC agnostic
    PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr   
        createFilteringSceneIndexDataBase(const std::shared_ptr<Fvp::FilteringSceneIndexClient>& client) override;
};

}//end of MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_FILTERING_SCENE_INDEX_DATA_CONCRETE_FACTORY_H

