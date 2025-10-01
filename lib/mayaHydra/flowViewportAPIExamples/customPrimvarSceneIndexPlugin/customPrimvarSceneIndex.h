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

#ifndef MAYA_HYDRA_CUSTOM_PRIMVAR_SCENE_INDEX_H
#define MAYA_HYDRA_CUSTOM_PRIMVAR_SCENE_INDEX_H

#include <pxr/pxr.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

TF_DECLARE_REF_PTRS(HdCustomPrimvarSceneIndex);

/// Translate primvars present on the mesh prim.
class HdCustomPrimvarSceneIndex final : public HdSingleInputFilteringSceneIndexBase
{
public:
    static HdCustomPrimvarSceneIndexRefPtr New(const HdSceneIndexBaseRefPtr& inputSceneIndex);

public:
    // HdSceneIndex overrides
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;
    SdfPathVector    GetChildPrimPaths(const SdfPath& primPath) const override;

private:
    HdCustomPrimvarSceneIndex(const HdSceneIndexBaseRefPtr& inputSceneIndex);
    ~HdCustomPrimvarSceneIndex() override;

    // HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(
        const HdSceneIndexBase&                       sender,
        const HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    void _PrimsDirtied(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif
