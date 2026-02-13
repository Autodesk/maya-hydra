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
#ifndef FVP_ANIMATED_PRIM_INVALIDATION_SCENE_INDEX_H
#define FVP_ANIMATED_PRIM_INVALIDATION_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include "pxr/imaging/hd/filteringSceneIndex.h"
#include "pxr/base/tf/declarePtrs.h"

#include <set>
#include <map>
#include <vector>

namespace FVP_NS_DEF {

class AnimatedPrimInvalidationSceneIndex;
typedef PXR_NS::TfRefPtr<AnimatedPrimInvalidationSceneIndex> AnimatedPrimInvalidationSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const AnimatedPrimInvalidationSceneIndex> AnimatedPrimInvalidationSceneIndexConstRefPtr;

///
/// \class AnimatedPrimInvalidationSceneIndex
///
/// A filtering scene index that tracks animated prims and provides a method
/// to invalidate them when the animation time changes. This ensures that
/// time-dependent content is properly redrawn when the application's time changes.
///
class AnimatedPrimInvalidationSceneIndex :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<AnimatedPrimInvalidationSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static AnimatedPrimInvalidationSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex);

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

    /// Invalidates animated prims that have time samples at the specified current frame.
    /// Only prims with samples at the current frame (within tolerance) will be invalidated.
    /// GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame,
    /// so we query around 0.0 (current frame) with a tolerance to detect samples at that frame.
    /// @param currentFrame The current animation frame to check for time samples.
    /// This should be called when the animation time changes.
    FVP_API
    void InvalidateAnimatedPrimsAtCurrentFrame(double currentFrame);

    /// Sets the animation time range used for detecting animated prims.
    /// This should be called when Maya's animation range changes.
    /// @param refreshCache If true (default), refreshes the animated prims cache.
    ///                     If false, only updates the time range without refreshing.
    FVP_API
    void SetAnimationTimeRange(double startTime, double endTime, bool refreshCache = true);

    /// Gets the current animation time range.
    FVP_API
    void GetAnimationTimeRange(double& startTime, double& endTime) const;

    /// Refreshes the cache of animated prims based on the current time range.
    /// This should be called when the animation range changes.
    FVP_API
    void RefreshAnimatedPrimsCache();

    /// Clears the cache of animated prims without rebuilding it.
    /// This should be called during scene operations like "file new".
    FVP_API
    void ClearAnimatedPrimsCache();

protected:
    FVP_API
    AnimatedPrimInvalidationSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex);

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

private:
    /// Checks if a prim has time-varying attributes (is animated) within the current time range
    bool _IsPrimAnimated(const PXR_NS::SdfPath &primPath) const;

    /// Checks if a prim has time samples at the specified frame.
    /// Uses shutter offsets relative to the current frame (query around 0.0).
    /// Uses cached time-varying data source handles for performance.
    bool _IsPrimAnimatedAtFrame(const PXR_NS::SdfPath &primPath, double frame) const;

    /// Collects time-varying data source handles from a prim's data source tree.
    /// Stores them in the provided vector for later fast queries.
    void _CollectTimeVaryingDataSources(
        const PXR_NS::HdDataSourceBaseHandle &ds,
        std::vector<PXR_NS::HdSampledDataSourceHandle> &outSampledDataSources,
        int maxDepth = 10) const;

    /// Tracks the set of animated prim paths
    std::set<PXR_NS::SdfPath> _animatedPrims;
    
    /// Cache of time-varying data source handles per prim for fast per-frame queries.
    /// This avoids re-traversing the data source tree on every frame change.
    std::map<PXR_NS::SdfPath, std::vector<PXR_NS::HdSampledDataSourceHandle>> _primTimeVaryingDataSources;

    /// Animation time range
    double _animationStartTime = 0.0;
    double _animationEndTime = 0.0;
    
    /// Flag to track if the animation time range has been initialized.
    bool _isAnimationRangeInitialized = false;
};

} //end of namespace FVP_NS_DEF

#endif //FVP_ANIMATED_PRIM_INVALIDATION_SCENE_INDEX_H
