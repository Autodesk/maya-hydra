// Copyright 2026 Autodesk
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

#ifndef FLOW_VIEWPORT_WH_SI_SCENE_INDEX_PLUGIN_H
#define FLOW_VIEWPORT_WH_SI_SCENE_INDEX_PLUGIN_H

#include <flowViewport/api.h>
#include <flowViewport/fvpWireframeColorInterface.h>

#include <memory>

namespace FVP_NS_DEF {

// Set the real WireframeColorInterface implementation on the WhSi scene
// indices that were created via the SceneIndexAppendCallback. Before this
// is called, the scene indices use a deferred proxy that returns a default
// white wireframe color.
FVP_API void SetWhSiWireframeColorInterface(const std::shared_ptr<WireframeColorInterface>& wci);

} // namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_WH_SI_SCENE_INDEX_PLUGIN_H