// Copyright 2025 Autodesk
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
#ifndef FVP_PASS_FILTERING_SCENE_INDEX_H
#define FVP_PASS_FILTERING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <functional>


namespace FVP_NS_DEF {

class PassFilteringSceneIndex;
typedef PXR_NS::TfRefPtr<PassFilteringSceneIndex> PassFilteringSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const PassFilteringSceneIndex> PassFilteringSceneIndexConstRefPtr;

class PassFilteringSceneIndex
    :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<PassFilteringSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    using FilteringOutFn = std::function<bool(const PXR_NS::SdfPath& primPath)>;

    FVP_API
    static PassFilteringSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputScene, const FilteringOutFn& filteringOutFn);

    FVP_API
    ~PassFilteringSceneIndex() override = default;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

protected:
    FVP_API
    PassFilteringSceneIndex(
        PXR_NS::HdSceneIndexBaseRefPtr const& inputSceneIndex,
        const FilteringOutFn& filteringOutFn);

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;

    FVP_API
    bool _IsFilteredOut(const PXR_NS::SdfPath& primPath) const;

    const FilteringOutFn& _filteringOutFn;
};

} // namespace FVP_NS_DEF

#endif // FVP_PASS_FILTERING_SCENE_INDEX_H
