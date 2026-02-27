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
#include "flowViewport/fvpFramePassData.h"

#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>

#include <map>

namespace FVP_NS_DEF {

class PassFilteringSceneIndex;
typedef PXR_NS::TfRefPtr<PassFilteringSceneIndex> PassFilteringSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const PassFilteringSceneIndex> PassFilteringSceneIndexConstRefPtr;

/// \class PassFilteringSceneIndex
///
/// A scene index that filters out prims based on whether they are relevant
/// to the frame pass they are part of or not.
///
/// The filtering operates on a per-prim basis : filtering out a prim means
/// that only the prim data itself is filtered out, not its whole sub-tree.
/// Filtering out a prim does not mean its children are also filtered out.
/// This is why filtering and unfiltering are both done through PrimsAdded
/// notification rather than PrimsRemoved : a filtered prim is just
/// resynced to a prim with no type and no data source.
///
class PassFilteringSceneIndex
    :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<PassFilteringSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static PassFilteringSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputScene,
        const Fvp::FramePassConstDataPtr&    framePassData);

    FVP_API
    ~PassFilteringSceneIndex() override = default;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    void DirtyPrimsFromPurposeRenderTag(const PXR_NS::TfToken purposeRenderTag);

protected:

    FVP_API
    PassFilteringSceneIndex(
        PXR_NS::HdSceneIndexBaseRefPtr const& inputSceneIndex,
        const Fvp::FramePassConstDataPtr&    framePassData);

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
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

    FVP_API
    bool _IsFilteredOut(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    bool _ShouldBeFilteredOut(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    PXR_NS::HdSceneIndexObserver::AddedPrimEntries _UpdateFilteringStatus(const PXR_NS::SdfPath& primPath, bool dirtied = true, bool resync = false);

    FVP_API
    PXR_NS::HdSceneIndexObserver::AddedPrimEntries _RemoveMaterialEntry(const PXR_NS::SdfPath& primPath);

    FVP_API
    PXR_NS::HdSceneIndexObserver::AddedPrimEntries _UpdateMaterialEntry(const PXR_NS::SdfPath& primPath);

    Fvp::FramePassConstDataPtr _framePassData;

private:
    PXR_NS::SdfPathSet _filteredPrims;

    // Used to track the materials required by the prims that will actually be rendered in the pass 
    std::map<PXR_NS::SdfPath, PXR_NS::SdfPath> _primsToMaterialPaths;
    std::map<PXR_NS::SdfPath, int> _materialUseCounts;
    std::set<PXR_NS::SdfPath> _generativeProceduralPaths;
};

} // namespace FVP_NS_DEF

#endif // FVP_PASS_FILTERING_SCENE_INDEX_H
