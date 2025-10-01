//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#include "customPrimvarSceneIndexPlugin.h"
#include "customPrimvarSceneIndex.h"

#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    ((sceneIndexPluginName, "HdCustomPrimvarSceneIndexPlugin"))
);

////////////////////////////////////////////////////////////////////////////////
// Plugin registrations
////////////////////////////////////////////////////////////////////////////////

TF_REGISTRY_FUNCTION(TfType)
{
    HdSceneIndexPluginRegistry::Define<HdCustomPrimvarSceneIndexPlugin>();
}

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    const HdSceneIndexPluginRegistry::InsertionPhase insertionPhase = 0; 

    // Register the plugins conditionally.
    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        "Arnold",
        _tokens->sceneIndexPluginName,
        nullptr, // no argument data necessary
        insertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtEnd);
}

////////////////////////////////////////////////////////////////////////////////
// Scene Index Implementations
////////////////////////////////////////////////////////////////////////////////

HdCustomPrimvarSceneIndexPlugin::
HdCustomPrimvarSceneIndexPlugin() = default;

HdSceneIndexBaseRefPtr
HdCustomPrimvarSceneIndexPlugin::_AppendSceneIndex(
    const HdSceneIndexBaseRefPtr &inputScene,
    const HdContainerDataSourceHandle &inputArgs)
{
    TF_UNUSED(inputArgs);
    return HdCustomPrimvarSceneIndex::New(inputScene);
}

} // namespace MAYAHYDRA_NS_DEF
