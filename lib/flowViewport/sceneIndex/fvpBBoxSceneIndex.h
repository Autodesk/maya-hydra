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
#ifndef FLOW_VIEWPORT_SCENEINDEX_FVP_BBOX_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENEINDEX_FVP_BBOX_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

class BboxSceneIndex;
typedef PXR_NS::TfRefPtr<BboxSceneIndex> BboxSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const BboxSceneIndex> BboxSceneIndexConstRefPtr;

/// \class BboxSceneIndex
///
/// A filtering scene index that converts geometries into a bounding box using the extent attribute. 
/// If the extent attribute is not present, it does not draw anything for that prim.
///
class BboxSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<BboxSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static BboxSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex){
        return PXR_NS::TfCreateRefPtr(new BboxSceneIndex(inputSceneIndex));
    }

    // From HdSceneIndexBase
    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;
    
    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }
    
    FVP_API
    ~BboxSceneIndex() override = default;

protected:
    BboxSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex) : ParentClass(inputSceneIndex), InputSceneIndexUtils(inputSceneIndex) {};

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;
    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)override{
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        _SendPrimsDirtied(entries);
    }
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENEINDEX_FVP_BBOX_SCENE_INDEX_H
