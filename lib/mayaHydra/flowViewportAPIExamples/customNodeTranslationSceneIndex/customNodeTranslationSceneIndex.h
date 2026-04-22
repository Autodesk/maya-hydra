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
// Example filtering scene index that shows how a render delegate would
// consume mayaCustomDagNode prims produced by maya-hydra's custom adapter
// and translate them into renderer-specific Hydra prim types.
//
// This code has ZERO dependency on maya-hydra headers or libraries. It only
// uses standard OpenUSD APIs. The token names (mayaCustomDagNode, mayaNode,
// mayaTypeName, mayaAttributes) are a documented convention.
//
// Arnold (MtoA) is used here as a concrete illustration, translating
// aiPhotometricLight into a UsdLux sphereLight, but the same pattern applies
// to any render delegate (RenderMan, custom engines, etc.).
// Each render delegate would provide its own scene index plugin registered
// for its renderer name, handling the Maya node types it cares about and
// passing through the rest.
//

#ifndef MAYA_HYDRA_CUSTOM_NODE_TRANSLATION_SCENE_INDEX_H
#define MAYA_HYDRA_CUSTOM_NODE_TRANSLATION_SCENE_INDEX_H

#include <pxr/pxr.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

TF_DECLARE_REF_PTRS(HdCustomNodeTranslationSceneIndex);

/// Example filtering scene index that translates mayaCustomDagNode prims
/// into renderer-specific types. Demonstrates how any render delegate would
/// consume custom Maya plugin nodes without any maya-hydra dependency.
/// Arnold is used as an example; the same approach works for any renderer.
class HdCustomNodeTranslationSceneIndex final
    : public HdSingleInputFilteringSceneIndexBase
{
public:
    static HdCustomNodeTranslationSceneIndexRefPtr New(
        const HdSceneIndexBaseRefPtr& inputSceneIndex);

    /// Intercepts mayaCustomDagNode prims and translates recognized Maya node
    /// types into renderer-specific Hydra types. Unrecognized types are
    /// passed through unchanged.
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;

    /// Delegates to the input scene index (no child path changes).
    SdfPathVector    GetChildPrimPaths(const SdfPath& primPath) const override;

private:
    HdCustomNodeTranslationSceneIndex(
        const HdSceneIndexBaseRefPtr& inputSceneIndex);
    ~HdCustomNodeTranslationSceneIndex() override;

    /// Translates an aiPhotometricLight's Maya attributes into a sphereLight
    /// prim with UsdLux-compatible data sources. The Arnold render delegate
    /// detects inputs:shaping:ies:file and creates a photometric_light node.
    HdSceneIndexPrim _TranslatePhotometricLight(
        const HdSceneIndexPrim& inputPrim,
        const VtDictionary& mayaAttrs) const;

    /// Forwards prim-added notices, re-typing mayaCustomDagNode entries to
    /// their translated type so downstream observers see the correct type.
    void _PrimsAdded(
        const HdSceneIndexBase&                       sender,
        const HdSceneIndexObserver::AddedPrimEntries& entries) override;

    /// Forwards prim-removed notices unchanged.
    void _PrimsRemoved(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    /// Forwards prim-dirtied notices unchanged.
    void _PrimsDirtied(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_HYDRA_CUSTOM_NODE_TRANSLATION_SCENE_INDEX_H
