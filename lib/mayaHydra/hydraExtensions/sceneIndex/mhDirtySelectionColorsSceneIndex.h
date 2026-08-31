//
// Copyright 2024 Autodesk
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
#ifndef MAYA_HYDRA_SCENE_INDEX_DIRTY_SELECTION_COLORS_SCENE_INDEX_H
#define MAYA_HYDRA_SCENE_INDEX_DIRTY_SELECTION_COLORS_SCENE_INDEX_H

//MayaHydra headers
#include "mayaHydraLib/api.h"

// Flow Viewport Toolkit headers.
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/selection/fvpSelectionTypes.h"

//Usd/Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>


namespace MAYAHYDRA_NS_DEF {

class MhDirtySelectionColorsSceneIndex;
typedef PXR_NS::TfRefPtr<MhDirtySelectionColorsSceneIndex> MhDirtySelectionColorsSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MhDirtySelectionColorsSceneIndex> MhDirtySelectionColorsSceneIndexConstRefPtr;


/// \class MhDirtySelectionColorsSceneIndex
/// Invalidates the wireframe colors of prims whose selection state changed, so they re-pull a
/// color that reflects it. A color is only re-pulled when its prim is dirtied, and nothing else
/// does that on a selection change, so without this a marquee selection leaves every non-lead
/// prim miscolored.
///
/// dirtyLeadObjectRelatedSelections() handles a change of which object is the lead;
/// dirtySelectionRelatedPrims() a change of what is selected.
class MhDirtySelectionColorsSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MhDirtySelectionColorsSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    static MhDirtySelectionColorsSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex){
        return PXR_NS::TfCreateRefPtr(new MhDirtySelectionColorsSceneIndex(inputSceneIndex));
    }

    // From HdSceneIndexBase
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetPrim(primPath);
    }
    
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~MhDirtySelectionColorsSceneIndex() override = default;

    MAYAHYDRALIB_API
    void dirtyLeadObjectRelatedSelections(const Fvp::PrimSelections& previousLeadObjectPrimSelections, const Fvp::PrimSelections& currentLeadObjectPrimSelections);

    /// Call with the prims that were selected or deselected: both need re-pulling, one to pick up
    /// the selection color and one to drop it.
    MAYAHYDRALIB_API
    void dirtySelectionRelatedPrims(const PXR_NS::SdfPathVector& primPaths);

protected:
    
    MhDirtySelectionColorsSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex) 
    : ParentClass(inputSceneIndex), InputSceneIndexUtils(inputSceneIndex){}

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override{
        if (!_IsObserved())return;
        _SendPrimsAdded(entries);
    }
    
    void _PrimsDirtied(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsDirtied(entries);
    }

    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override{
        if (!_IsObserved())return;
        _SendPrimsRemoved(entries);
    }

    MAYAHYDRALIB_API
    void _DirtyPrimPathRecursively(const PXR_NS::SdfPath& primPath, PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& inoutDirtiedPrimEntries)const;
};

} // namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_SCENE_INDEX_DIRTY_SELECTION_COLORS_SCENE_INDEX_H
