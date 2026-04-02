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
// Example HdSceneIndexPlugin that registers HdGenericNodeTranslationSceneIndex
// for a specific renderer. This example uses Arnold, but the same approach
// works for any render delegate: replace the renderer name in
// RegisterSceneIndexForRenderer and handle the Maya node types relevant to
// that renderer. The plugin is discovered by Hydra through plugInfo.json and
// automatically inserts the filtering scene index into the scene index chain
// when the matching render delegate is active.
//

#ifndef MAYA_HYDRA_GENERIC_NODE_TRANSLATION_SCENE_INDEX_PLUGIN_H
#define MAYA_HYDRA_GENERIC_NODE_TRANSLATION_SCENE_INDEX_PLUGIN_H

#include <pxr/pxr.h>
#include <pxr/imaging/hd/sceneIndexPlugin.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

/// \class HdGenericNodeTranslationSceneIndexPlugin
///
/// Example Hydra scene index plugin that translates mayaCustomDagNode prims
/// into renderer-specific Hydra prim types. Arnold (aiPhotometricLight) is
/// used as an example; any render delegate would create its own plugin
/// following this same pattern.
///
class HdGenericNodeTranslationSceneIndexPlugin : public HdSceneIndexPlugin
{
public:
    HdGenericNodeTranslationSceneIndexPlugin();

protected:
    /// Creates and returns the HdGenericNodeTranslationSceneIndex wrapping
    /// the input scene.
    HdSceneIndexBaseRefPtr _AppendSceneIndex(
        const HdSceneIndexBaseRefPtr& inputScene,
        const HdContainerDataSourceHandle& inputArgs) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_HYDRA_GENERIC_NODE_TRANSLATION_SCENE_INDEX_PLUGIN_H
