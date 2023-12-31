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
#include "fvpFilteringSceneIndexClientExample.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

FilteringSceneIndexClientExample::FilteringSceneIndexClientExample(const std::string& displayName, const Category category, const std::string& rendererNames, void* dccNode):
    FilteringSceneIndexClient(displayName, category, rendererNames, dccNode)
{
}

//Callback to be able to append a new scene index or scene index chain to this Hydra viewport scene index
HdSceneIndexBaseRefPtr FilteringSceneIndexClientExample::appendSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex, const HdContainerDataSourceHandle& inputArgs)
{
    if (inputSceneIndex){
        //Add a filtering scene index, this will hide some prims matching some criteria.
         auto filteringSceneIndex = PXR_NS::FVP_NS_DEF::FilteringSceneIndexExample::New(inputSceneIndex);
    
         //return the new appended scene index, if you don't want to append a scene index, just return _hydraViewportInputSceneIndex
        return filteringSceneIndex;
    }

    return inputSceneIndex;
}

} //End of namespace FVP_NS_DEF