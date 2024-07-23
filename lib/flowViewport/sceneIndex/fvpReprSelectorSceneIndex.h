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
#ifndef FLOW_VIEWPORT_SCENEINDEX_FVP_REPRSELECTOR_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENEINDEX_FVP_REPRSELECTOR_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>

namespace FVP_NS_DEF {

class ReprSelectorSceneIndex;
typedef PXR_NS::TfRefPtr<ReprSelectorSceneIndex> ReprSelectorSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const ReprSelectorSceneIndex> ReprSelectorSceneIndexConstRefPtr;

/// \class ReprSelectorSceneIndex
///
/// A filtering scene index that applies a different RepSelector on geometries (such as wireframe or wireframe on shaded). 
///
class ReprSelectorSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<ReprSelectorSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    enum class RepSelectorType{
        WireframeRefined, //Refined wireframe (refined means that it supports a "refineLevel" attribute in the displayStyle to get a more refined drawing, valid range is from 0 to 8)
        WireframeOnSurface, //Wireframe on surface not refined
        WireframeOnSurfaceRefined,//Wireframe on surface refined
        None,
    };

    FVP_API
    static ReprSelectorSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface){
        return PXR_NS::TfCreateRefPtr(new ReprSelectorSceneIndex(inputSceneIndex, wireframeColorInterface));
    }

    // From HdSceneIndexBase
    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;
    
    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }
    
    FVP_API
    ~ReprSelectorSceneIndex() override = default;

    FVP_API
    void addExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot) { 
        _excludedSceneRoots.emplace(sceneRoot);
    }
    
    FVP_API
    void SetReprType(RepSelectorType, bool);

protected:
    
ReprSelectorSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface);

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override{
        if (!_IsObserved())return;
        _SendPrimsAdded(entries);
    }
    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsDirtied(entries);
    }

    bool _isExcluded(const PXR_NS::SdfPath& sceneRoot) const { 
        for (const auto& excluded : _excludedSceneRoots) {
            if (sceneRoot.HasPrefix(excluded)) {
                return true;
            }
        }
        return false;
    }

    std::set<PXR_NS::SdfPath> _excludedSceneRoots;
    
    void _DirtyAllPrims(const PXR_NS::HdDataSourceLocatorSet locators);
    bool _needsReprChanged {false};

    PXR_NS::HdRetainedContainerDataSourceHandle _wireframeTypeDataSource = nullptr;
    std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENEINDEX_FVP_REPRSELECTOR_SCENE_INDEX_H
