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
#include "fvpFilteringSceneIndexDataBase.h"
#include "flowViewport/API/fvpFilteringSceneIndexClient.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h"

PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

FilteringSceneIndexDataBase::FilteringSceneIndexDataBase(const std::shared_ptr<::Fvp::FilteringSceneIndexClient>& filteringSIClient) 
    : _client {filteringSIClient}
{
}

void FilteringSceneIndexDataBase::SetVisibility(bool isVisible)
{
    _isVisible = isVisible;
    const std::string& rendererNames = _client->getRendererNames();
    ::Fvp::FilteringSceneIndicesChainManager::get().updateFilteringSceneIndicesChain(rendererNames);
}

}//End of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE