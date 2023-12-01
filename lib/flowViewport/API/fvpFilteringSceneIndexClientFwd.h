//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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


/// Is the forward declaration of a FilteringSceneIndexClient

#ifndef FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_FWD_H
#define FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_FWD_H

#include "flowViewport/api.h"

#include <memory>

//Is the forward declaration of the FilteringSceneIndexClient class and FilteringSceneIndexClientPtr
namespace FVP_NS_DEF
{
    class FilteringSceneIndexClient;
    using FilteringSceneIndexClientPtr = std::shared_ptr<FilteringSceneIndexClient>;
    
}//end of namespace

#endif //FLOW_VIEWPORT_API_FILTERING_SCENE_INDEX_CLIENT_FWD_H
