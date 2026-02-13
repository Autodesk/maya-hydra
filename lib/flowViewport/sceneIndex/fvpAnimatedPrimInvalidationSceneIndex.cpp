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

#include "flowViewport/sceneIndex/fvpAnimatedPrimInvalidationSceneIndex.h"

#include "pxr/pxr.h"
#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/xformSchema.h"
#include "pxr/imaging/hd/meshSchema.h"
#include "pxr/imaging/hd/primvarsSchema.h"
#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/sceneIndexPrimView.h"
#include "pxr/usdImaging/usdImaging/usdPrimInfoSchema.h"

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

AnimatedPrimInvalidationSceneIndexRefPtr
AnimatedPrimInvalidationSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
{
    return TfCreateRefPtr(
        new AnimatedPrimInvalidationSceneIndex(inputSceneIndex));
}

AnimatedPrimInvalidationSceneIndex::
AnimatedPrimInvalidationSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
  , InputSceneIndexUtils(inputSceneIndex)
  , _animationStartTime(0.0)
  , _animationEndTime(0.0)
{
}

HdSceneIndexPrim
AnimatedPrimInvalidationSceneIndex::GetPrim(
    const SdfPath &primPath) const
{
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector
AnimatedPrimInvalidationSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void AnimatedPrimInvalidationSceneIndex::InvalidateAnimatedPrimsAtCurrentFrame(double currentFrame)
{
    if (!_IsObserved() || _animatedPrims.empty()) {
        return;
    }

    // Mark animated prims dirty with an empty locator set, which means dirty everything.
    // Only invalidate prims that are actually animated at the current frame.
    static const HdDataSourceLocatorSet locators;

    HdSceneIndexObserver::DirtiedPrimEntries entries;
    entries.reserve(_animatedPrims.size());
    
    for (const SdfPath &primPath : _animatedPrims) {
        // Only invalidate if this prim has time samples at the current frame
        if (_IsPrimAnimatedAtFrame(primPath, currentFrame)) {
            entries.push_back({primPath, locators});
        }
    }

    if (!entries.empty()) {
        _SendPrimsDirtied(entries);
    }
}

void AnimatedPrimInvalidationSceneIndex::SetAnimationTimeRange(double startTime, double endTime, bool refreshCache)
{
    if (!_isAnimationRangeInitialized || _animationStartTime != startTime || _animationEndTime != endTime) {
        _animationStartTime = startTime;
        _animationEndTime = endTime;
        _isAnimationRangeInitialized = true;
        // Refresh the cache when the range changes, unless explicitly disabled
        if (refreshCache) {
            RefreshAnimatedPrimsCache();
        }
    }
}

void AnimatedPrimInvalidationSceneIndex::GetAnimationTimeRange(double& startTime, double& endTime) const
{
    startTime = _animationStartTime;
    endTime = _animationEndTime;
}

void AnimatedPrimInvalidationSceneIndex::RefreshAnimatedPrimsCache()
{
    // Clear the current cache
    _animatedPrims.clear();

    // Check if input scene index is valid
    const HdSceneIndexBaseRefPtr inputSceneIndex = GetInputSceneIndex();
    if (!inputSceneIndex) {
        return;
    }

    // Re-check all prims in the scene index to rebuild the cache
    for (const SdfPath &primPath : HdSceneIndexPrimView(inputSceneIndex)) {
        if (_IsPrimAnimated(primPath)) {
            _animatedPrims.insert(primPath);
        }
    }
}

void AnimatedPrimInvalidationSceneIndex::ClearAnimatedPrimsCache()
{
    // Simply clear the cache without rebuilding it
    _animatedPrims.clear();
}

namespace
{
    // Helper function to recursively check if a data source has time-varying samples
    // within the specified time range
    bool _HasTimeVaryingSamples(
        const HdDataSourceBaseHandle &ds, 
        float startTime, 
        float endTime,
        int maxDepth = 10)
    {
        if (!ds || maxDepth <= 0) {
            return false;
        }

        // Check if this is a sampled data source with time samples
        if (HdSampledDataSourceHandle sampledDs = HdSampledDataSource::Cast(ds)) {
            std::vector<float> sampleTimes;
            if (sampledDs->GetContributingSampleTimesForInterval(
                    startTime, endTime, &sampleTimes) && !sampleTimes.empty()) {
                return true;
            }
        }

        // Recursively check container data sources
        if (HdContainerDataSourceHandle containerDs = 
            HdContainerDataSource::Cast(ds)) {
            for (const TfToken &name : containerDs->GetNames()) {
                if (HdDataSourceBaseHandle childDs = containerDs->Get(name)) {
                    if (_HasTimeVaryingSamples(childDs, startTime, endTime, maxDepth - 1)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    // Helper function to check if a data source has time samples at a specific frame
    bool _HasTimeSamplesAtFrame(
        const HdDataSourceBaseHandle &ds,
        float frame,
        float tolerance,
        int maxDepth = 10)
    {
        if (!ds || maxDepth <= 0) {
            return false;
        }

        // Check if this is a sampled data source with time samples near the frame
        if (HdSampledDataSourceHandle sampledDs = HdSampledDataSource::Cast(ds)) {
            std::vector<float> sampleTimes;
            // Check a small interval around the frame
            if (sampledDs->GetContributingSampleTimesForInterval(
                    frame - tolerance, frame + tolerance, &sampleTimes) && !sampleTimes.empty()) {
                return true;
            }
        }

        // Recursively check container data sources
        if (HdContainerDataSourceHandle containerDs = 
            HdContainerDataSource::Cast(ds)) {
            for (const TfToken &name : containerDs->GetNames()) {
                if (HdDataSourceBaseHandle childDs = containerDs->Get(name)) {
                    if (_HasTimeSamplesAtFrame(childDs, frame, tolerance, maxDepth - 1)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }
}

bool
AnimatedPrimInvalidationSceneIndex::_IsPrimAnimated(
    const SdfPath &primPath) const
{
    const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!prim.dataSource) {
        return false;
    }

    // Use the actual animation time range
    float startTime = static_cast<float>(_animationStartTime);
    float endTime = static_cast<float>(_animationEndTime);
    
    // If range is not initialized, use a default wide range
    // This allows us to distinguish between an uninitialized range and a legitimate [0, 0] range
    if (!_isAnimationRangeInitialized) {
        startTime = -1000.0f;
        endTime = 1000.0f;
    }

    // Recursively check all data sources in the prim for time-varying samples
    // within the animation range. This catches all types of animated attributes:
    // transforms, points, primvars, materials, lights, cameras, and any other time-varying data
    return _HasTimeVaryingSamples(prim.dataSource, startTime, endTime);
}

bool
AnimatedPrimInvalidationSceneIndex::_IsPrimAnimatedAtFrame(
    const SdfPath &primPath,
    double frame) const
{
    const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (!prim.dataSource) {
        return false;
    }

    // Check if this prim has time samples at the specified frame
    // Use a small tolerance (0.1 frames) to account for floating point precision
    const float frameFloat = static_cast<float>(frame);
    constexpr float tolerance = 0.1f;

    return _HasTimeSamplesAtFrame(prim.dataSource, frameFloat, tolerance);
}

void
AnimatedPrimInvalidationSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    // Check each added prim to see if it's animated
    for (const auto &entry : entries) {
        if (_IsPrimAnimated(entry.primPath)) {
            _animatedPrims.insert(entry.primPath);
        }
    }

    _SendPrimsAdded(entries);
}

void
AnimatedPrimInvalidationSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    // Remove prims from our tracking set
    for (const auto &entry : entries) {
        _animatedPrims.erase(entry.primPath);
    }

    _SendPrimsRemoved(entries);
}

void
AnimatedPrimInvalidationSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    // Re-check dirtied prims to update the animation cache
    // Handle both cases: prims that become animated and prims that are no longer animated
    for (const auto &entry : entries) {
        const bool isCurrentlyAnimated = _IsPrimAnimated(entry.primPath);
        const bool wasInCache = _animatedPrims.find(entry.primPath) != _animatedPrims.end();

        if (isCurrentlyAnimated && !wasInCache) {
            // Prim became animated - add to cache
            _animatedPrims.insert(entry.primPath);
        } else if (!isCurrentlyAnimated && wasInCache) {
            // Prim was animated but is no longer animated - remove from cache
            _animatedPrims.erase(entry.primPath);
        }
        // If both are true or both are false, no change needed
    }

    _SendPrimsDirtied(entries);
}

} //end of namespace FVP_NS_DEF
