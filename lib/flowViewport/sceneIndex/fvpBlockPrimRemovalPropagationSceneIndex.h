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
#ifndef FLOW_VIEWPORT_SCENEINDEX_FVP_BLOCK_PRIM_REMOVAL_PROPAGATION_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENEINDEX_FVP_BLOCK_PRIM_REMOVAL_PROPAGATION_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"

//Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

class BlockPrimRemovalPropagationSceneIndex;
typedef PXR_NS::TfRefPtr<BlockPrimRemovalPropagationSceneIndex> BlockPrimRemovalPropagationSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const BlockPrimRemovalPropagationSceneIndex> BlockPrimRemovalPropagationSceneIndexConstRefPtr;

/// \class BlockPrimRemovalPropagationSceneIndex
///
/// A filtering scene index that blocks prim removal propagation. Example usage is : we are re-creating
/// the filtering scene index chain hierarchy and don't want the PrimRemoval to propagate to the linked 
/// scene index.
class BlockPrimRemovalPropagationSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase, public PathInterface //As a temp workaround we subclass PathInterface
    , public Fvp::InputSceneIndexUtils<BlockPrimRemovalPropagationSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static BlockPrimRemovalPropagationSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex){
        return PXR_NS::TfCreateRefPtr(new BlockPrimRemovalPropagationSceneIndex(inputSceneIndex));
    }

    // From HdSceneIndexBase
    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetPrim(primPath);
    }
    
    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    FVP_API
    ~BlockPrimRemovalPropagationSceneIndex() override = default;

    FVP_API
    bool isPrimRemovalBlocked() const { return _blockPrimRemoval;}

    FVP_API
    void setPrimRemovalBlocked(bool blockPrimRemoval) { _blockPrimRemoval = blockPrimRemoval; }

    //from PathInterface
    FVP_API
    PrimSelections ConvertUfePathToHydraSelections(const Ufe::Path& appPath) const override{
        return _pathInterface->ConvertUfePathToHydraSelections(appPath);
    }

protected:
    FVP_API
    BlockPrimRemovalPropagationSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override{
        if (!_IsObserved())return;
        _SendPrimsAdded(entries);
    }
    
    void _PrimsDirtied(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsDirtied(entries);
    }

    FVP_API
    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)override;

    bool _blockPrimRemoval {false};
    const PathInterface* _pathInterface {nullptr};
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENEINDEX_FVP_BLOCK_PRIM_REMOVAL_PROPAGATION_SCENE_INDEX_H
