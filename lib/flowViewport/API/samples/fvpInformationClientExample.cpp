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
#include "fvpInformationClientExample.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

InformationClientExample::~InformationClientExample()
{ 
}

//Callback to be able to act when a Hydra viewport scene index was added, typical use case is a hydra viewport was created.
void InformationClientExample::SceneIndexAdded(const InformationInterface::ViewportInformation& viewportInformation)
{
    
}

//Callback to be able to act when a Hydra viewport scene index was removed, typical use case is a hydra viewport was removed.
void InformationClientExample::SceneIndexRemoved(const InformationInterface::ViewportInformation& viewportInformation)
{
}

} //End of namespace FVP_NS_DEF