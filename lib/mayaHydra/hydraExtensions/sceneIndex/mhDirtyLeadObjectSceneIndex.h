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
#ifndef MAYA_HYDRA_SCENE_INDEX_DIRTY_PREVIOUS_LEAD_OBJECT_SCENE_INDEX_H
#define MAYA_HYDRA_SCENE_INDEX_DIRTY_PREVIOUS_LEAD_OBJECT_SCENE_INDEX_H

//MayaHydra headers
#include "mayaHydraLib/api.h"

// Flow Viewport Toolkit headers.
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/selection/fvpSelectionTypes.h"

//Usd/Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>


namespace MAYAHYDRA_NS_DEF {

class MhDirtyLeadObjectSceneIndex;
typedef PXR_NS::TfRefPtr<MhDirtyLeadObjectSceneIndex> MhDirtyLeadObjectSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MhDirtyLeadObjectSceneIndex> MhDirtyLeadObjectSceneIndexConstRefPtr;


/// \class MhDirtyLeadObjectSceneIndex
/// This class is responsible for invalidating the wireframe colors of prims whose selection state
/// changed, so they re-pull a color that reflects it.
///
/// Two entry points, both needed: dirtyLeadObjectRelatedSelections() for a change of *which* object is
/// the lead, and dirtySelectionRelatedPrims() for a change of *what* is selected. The second exists
/// because the wireframe color is only re-pulled when a prim is dirtied, and nothing else does it on a
/// selection change -- without it, only the lead object ever showed the right color, so a marquee
/// selection left every non-lead prim stale.
///
/// The name is now narrower than the responsibility; kept for the moment to avoid churn.
class MhDirtyLeadObjectSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MhDirtyLeadObjectSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    static MhDirtyLeadObjectSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex){
        return PXR_NS::TfCreateRefPtr(new MhDirtyLeadObjectSceneIndex(inputSceneIndex));
    }

    // From HdSceneIndexBase
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetPrim(primPath);
    }
    
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~MhDirtyLeadObjectSceneIndex() override = default;

    MAYAHYDRALIB_API
    void dirtyLeadObjectRelatedSelections(const Fvp::PrimSelections& previousLeadObjectPrimSelections, const Fvp::PrimSelections& currentLeadObjectPrimSelections);

    /// Invalidate the wireframe colors of \p primPaths, for prims whose selection state just changed.
    ///
    /// Call with the prims that were selected *or* deselected -- both need re-pulling, one to pick up
    /// the selection color and one to drop it. Bounded by the size of the selection, not the scene.
    MAYAHYDRALIB_API
    void dirtySelectionRelatedPrims(const PXR_NS::SdfPathVector& primPaths);

protected:
    
    MhDirtyLeadObjectSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex) 
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
    void _DirtyPrimSelectionRecursively(const Fvp::PrimSelection& primSelection, PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& inoutDirtiedPrimEntries)const;
};

} // namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_SCENE_INDEX_DIRTY_PREVIOUS_LEAD_OBJECT_SCENE_INDEX_H
