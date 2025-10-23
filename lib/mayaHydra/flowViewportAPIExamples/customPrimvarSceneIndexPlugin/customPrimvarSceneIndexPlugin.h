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

#ifndef MAYA_HYDRA_CUSTOM_PRIMVAR_SCENE_INDEX_PLUGIN_H
#define MAYA_HYDRA_CUSTOM_PRIMVAR_SCENE_INDEX_PLUGIN_H

#include <pxr/pxr.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

/// \class HdCustomPrimvarSceneIndexPlugin
///
/// Hydra scene index plugin that remap custom primvars
///
class HdCustomPrimvarSceneIndexPlugin :
    public HdSceneIndexPlugin
{
public:
    HdCustomPrimvarSceneIndexPlugin();

protected:
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr &inputScene,
        const HdContainerDataSourceHandle &inputArgs) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_CUSTOM_PRIMVAR_SCENE_INDEX_PLUGIN_H
