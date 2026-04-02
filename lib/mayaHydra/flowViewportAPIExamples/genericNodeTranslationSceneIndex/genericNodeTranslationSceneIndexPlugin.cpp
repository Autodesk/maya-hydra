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
//
// Registers HdGenericNodeTranslationSceneIndex as a scene index plugin for
// the "Arnold" renderer via HdSceneIndexPluginRegistry::RegisterSceneIndexForRenderer.
// To adapt this example for another render delegate, change the renderer name
// string ("Arnold") to the target renderer's registered name (e.g., "Prman")
// and handle the relevant Maya node types in the
// filtering scene index.
//

#include "genericNodeTranslationSceneIndexPlugin.h"
#include "genericNodeTranslationSceneIndex.h"

#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HdGenericNodeTranslationSceneIndexPlugin"))
);

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry::Define<HdGenericNodeTranslationSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 0;

    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        "Arnold",
        _tokens->sceneIndexPluginName,
        nullptr,
        insertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtEnd);
}

HdGenericNodeTranslationSceneIndexPlugin::
HdGenericNodeTranslationSceneIndexPlugin() = default;

HdSceneIndexBaseRefPtr
HdGenericNodeTranslationSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr& inputScene,
    const HdContainerDataSourceHandle& inputArgs)
{
    return HdGenericNodeTranslationSceneIndex::New(inputScene);
}

} // namespace MAYAHYDRA_NS_DEF
