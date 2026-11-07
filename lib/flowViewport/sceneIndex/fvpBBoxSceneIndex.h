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
#include "flowViewport/fvpWireframeColorInterface.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <functional>
#include <optional>

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
    using PrimPredicate = std::function<bool(const PXR_NS::SdfPath&)>;
    using ExtentProvider
        = std::function<std::optional<PXR_NS::GfRange3d>(const PXR_NS::SdfPath&)>;
    using ChildPrimPathsProvider
        = std::function<PXR_NS::SdfPathVector(const PXR_NS::SdfPath&)>;
    using PrimDataSourceProvider
        = std::function<PXR_NS::HdContainerDataSourceHandle(const PXR_NS::SdfPath&)>;
    using WireframeColorProvider
        = std::function<PXR_NS::GfVec4f(const PXR_NS::SdfPath&)>;

    enum class Mode {
        Geometries,
        EmptyModelsWithExtents
    };

    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static BboxSceneIndexRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        Mode mode = Mode::Geometries,
        PrimPredicate primPredicate = {},
        WireframeColorProvider wireframeColorProvider = {},
        ExtentProvider extentProvider = {},
        ChildPrimPathsProvider childPrimPathsProvider = {},
        PrimDataSourceProvider primDataSourceProvider = {}){
        return PXR_NS::TfCreateRefPtr(new BboxSceneIndex(
            inputSceneIndex,
            wireframeColorInterface,
            mode,
            std::move(primPredicate),
            std::move(wireframeColorProvider),
            std::move(extentProvider),
            std::move(childPrimPathsProvider),
            std::move(primDataSourceProvider)));
    }

    // From HdSceneIndexBase
    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;
    
    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;
    
    FVP_API
    void Enable(bool enable);
    
    FVP_API
    bool Enabled() const { return _enabled; }

    FVP_API
    ~BboxSceneIndex() override = default;

    FVP_API
    void AddExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot) { 
        _excludedSceneRoots.emplace(sceneRoot);
    }

    FVP_API
    void RemoveStaleSyntheticPrims(bool sendNotifications = true);

    FVP_API
    void AddNewSyntheticPrims(bool sendNotifications = true);

    FVP_API
    void RefreshSyntheticPrims(bool sendNotifications = true);

protected:
    BboxSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        Mode mode,
        PrimPredicate primPredicate,
        WireframeColorProvider wireframeColorProvider,
        ExtentProvider extentProvider,
        ChildPrimPathsProvider childPrimPathsProvider,
        PrimDataSourceProvider primDataSourceProvider);

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
    
    bool _isExcluded(const PXR_NS::SdfPath& sceneRoot) const { 
        for (const auto& excluded : _excludedSceneRoots) {
            if (sceneRoot.HasPrefix(excluded)) {
                return true;
            }
        }
        return false;
    }

    bool _ShouldConvert(
        const PXR_NS::SdfPath& primPath,
        const PXR_NS::HdSceneIndexPrim& prim) const;


    PXR_NS::HdContainerDataSourceHandle _CreateBoundingBoxDataSource(
        const PXR_NS::SdfPath&  primPath,
        const PXR_NS::HdSceneIndexPrim& prim) const;

    void _DirtyAllPrims();

    bool                                     _enabled = false;
    std::set<PXR_NS::SdfPath> _excludedSceneRoots;
    std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;
    Mode _mode;
    PrimPredicate _primPredicate;
    WireframeColorProvider _wireframeColorProvider;
    ExtentProvider _extentProvider;
    ChildPrimPathsProvider _childPrimPathsProvider;
    PrimDataSourceProvider _primDataSourceProvider;
    std::set<PXR_NS::SdfPath> _syntheticPrimPaths;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENEINDEX_FVP_BBOX_SCENE_INDEX_H
