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
#include "pxr/imaging/hd/basisCurvesSchema.h"
#include "pxr/imaging/hd/dataSource.h"
#include "pxr/imaging/hd/sceneIndexPrimView.h"
#include "pxr/usdImaging/usdImaging/usdPrimInfoSchema.h"
#include <map>
#include <vector>

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

    // Mark animated prims dirty with explicit locators for all common animated attributes.
    // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame,
    // so we can check if a prim has samples at the current frame by querying around 0.0.
    // Only invalidate prims that actually have time samples at the current frame.
    static const HdDataSourceLocatorSet locators{
        HdXformSchema::GetDefaultLocator(),      // Transform animation
        HdMeshSchema::GetDefaultLocator(),        // Mesh topology/points animation
        HdPrimvarsSchema::GetDefaultLocator(),   // Primvar animation (points, normals, etc.)
        HdBasisCurvesSchema::GetDefaultLocator() // Curve animation
    };

    HdSceneIndexObserver::DirtiedPrimEntries entries;
    entries.reserve(_animatedPrims.size());
    
    for (const SdfPath &primPath : _animatedPrims) {
        // Check if this prim has time samples at the current frame
        // (using shutter offsets relative to currentFrame, query around 0.0)
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

    // Helper function to recursively collect time-varying sampled data source handles
    void _CollectTimeVaryingDataSourcesRecursive(
        const HdDataSourceBaseHandle &ds,
        std::vector<HdSampledDataSourceHandle> &outSampledDataSources,
        float startTime,
        float endTime,
        int maxDepth = 10)
    {
        if (!ds || maxDepth <= 0) {
            return;
        }

        // Check if this is a sampled data source with time samples
        if (HdSampledDataSourceHandle sampledDs = HdSampledDataSource::Cast(ds)) {
            std::vector<float> sampleTimes;
            if (sampledDs->GetContributingSampleTimesForInterval(
                    startTime, endTime, &sampleTimes) && !sampleTimes.empty()) {
                // This data source has time samples in the range, cache it
                outSampledDataSources.push_back(sampledDs);
            }
        }

        // Recursively check container data sources
        if (HdContainerDataSourceHandle containerDs = 
            HdContainerDataSource::Cast(ds)) {
            for (const TfToken &name : containerDs->GetNames()) {
                if (HdDataSourceBaseHandle childDs = containerDs->Get(name)) {
                    _CollectTimeVaryingDataSourcesRecursive(
                        childDs, outSampledDataSources, startTime, endTime, maxDepth - 1);
                }
            }
        }
    }

}

void AnimatedPrimInvalidationSceneIndex::RefreshAnimatedPrimsCache()
{
    if (!_IsObserved()) {
        return;
    }

    // Clear the existing caches
    _animatedPrims.clear();
    _primTimeVaryingDataSources.clear();

    // Determine the time range to use
    float startTime = static_cast<float>(_animationStartTime);
    float endTime = static_cast<float>(_animationEndTime);
    if (!_isAnimationRangeInitialized) {
        startTime = -1000.0f;
        endTime = 1000.0f;
    }

    // Rebuild the cache by checking all prims in the scene index
    const HdSceneIndexPrimView primView(GetInputSceneIndex());
    for (const SdfPath &primPath : primView) {
        const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
        if (!prim.dataSource) {
            continue;
        }

        // Collect time-varying data source handles for this prim
        std::vector<HdSampledDataSourceHandle> timeVaryingDataSources;
        _CollectTimeVaryingDataSourcesRecursive(
            prim.dataSource, timeVaryingDataSources, startTime, endTime);

        // If we found time-varying data sources, cache the prim and its data sources
        if (!timeVaryingDataSources.empty()) {
            _animatedPrims.insert(primPath);
            _primTimeVaryingDataSources[primPath] = std::move(timeVaryingDataSources);
        }
    }
}

void AnimatedPrimInvalidationSceneIndex::ClearAnimatedPrimsCache()
{
    // Simply clear the caches without rebuilding them
    _animatedPrims.clear();
    _primTimeVaryingDataSources.clear();
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
    if (!_isAnimationRangeInitialized) {
        startTime = -1000.0f;
        endTime = 1000.0f;
    }

    // Recursively check all data sources in the prim for time-varying samples
    // within the animation range. This catches all types of animated attributes:
    // transforms, points, primvars, materials, lights, cameras, and any other time-varying data
    return _HasTimeVaryingSamples(prim.dataSource, startTime, endTime);
}

namespace
{
    // Helper function to check if a data source has time samples at the current frame.
    // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame
    // (set in scene globals). Since the current frame is set before calling this function,
    // we query around 0.0 (current frame) with a tolerance to detect samples at that frame.
    bool _HasTimeSamplesAtCurrentFrame(
        const HdDataSourceBaseHandle &ds,
        float tolerance,
        int maxDepth = 10)
    {
        if (!ds || maxDepth <= 0) {
            return false;
        }

        // Query around 0.0 (current frame) with tolerance to detect samples at the current frame.
        // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame,
        // which should be set in scene globals before this is called.
        if (HdSampledDataSourceHandle sampledDs = HdSampledDataSource::Cast(ds)) {
            std::vector<float> sampleTimes;
            // Query shutter window [-tolerance, tolerance] relative to current frame (0.0)
            if (sampledDs->GetContributingSampleTimesForInterval(
                    -tolerance, tolerance, &sampleTimes) && !sampleTimes.empty()) {
                return true;
            }
        }

        // Recursively check container data sources
        if (HdContainerDataSourceHandle containerDs = 
            HdContainerDataSource::Cast(ds)) {
            for (const TfToken &name : containerDs->GetNames()) {
                if (HdDataSourceBaseHandle childDs = containerDs->Get(name)) {
                    if (_HasTimeSamplesAtCurrentFrame(childDs, tolerance, maxDepth - 1)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }
}

bool
AnimatedPrimInvalidationSceneIndex::_IsPrimAnimatedAtFrame(
    const SdfPath &primPath,
    double frame) const
{
    // Use cached time-varying data source handles for fast per-frame queries
    // instead of re-traversing the data source tree
    auto it = _primTimeVaryingDataSources.find(primPath);
    if (it == _primTimeVaryingDataSources.end()) {
        return false;
    }

    // Check if any of the cached time-varying data sources have samples at the current frame.
    // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame
    // (set in scene globals). The current frame should be set to 'frame' before this is called,
    // so we query around 0.0 (current frame) with a tolerance.
    // Use a small tolerance (0.1 frames) to account for floating point precision.
    constexpr float tolerance = 0.1f;

    for (const HdSampledDataSourceHandle &sampledDs : it->second) {
        if (!sampledDs) {
            continue;
        }
        std::vector<float> sampleTimes;
        // Query around 0.0 (current frame) with tolerance
        if (sampledDs->GetContributingSampleTimesForInterval(
                -tolerance, tolerance, &sampleTimes) && !sampleTimes.empty()) {
            return true;
        }
    }

    return false;
}

void
AnimatedPrimInvalidationSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    // Determine the time range to use
    float startTime = static_cast<float>(_animationStartTime);
    float endTime = static_cast<float>(_animationEndTime);
    if (!_isAnimationRangeInitialized) {
        startTime = -1000.0f;
        endTime = 1000.0f;
    }

    // Check each added prim to see if it's animated and cache its time-varying data sources
    for (const auto &entry : entries) {
        const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (!prim.dataSource) {
            continue;
        }

        // Collect time-varying data source handles for this prim
        std::vector<HdSampledDataSourceHandle> timeVaryingDataSources;
        _CollectTimeVaryingDataSourcesRecursive(
            prim.dataSource, timeVaryingDataSources, startTime, endTime);

        // If we found time-varying data sources, cache the prim and its data sources
        if (!timeVaryingDataSources.empty()) {
            _animatedPrims.insert(entry.primPath);
            _primTimeVaryingDataSources[entry.primPath] = std::move(timeVaryingDataSources);
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

    // Remove prims from both caches
    for (const auto &entry : entries) {
        _animatedPrims.erase(entry.primPath);
        _primTimeVaryingDataSources.erase(entry.primPath);
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

    // Determine the time range to use
    float startTime = static_cast<float>(_animationStartTime);
    float endTime = static_cast<float>(_animationEndTime);
    if (!_isAnimationRangeInitialized) {
        startTime = -1000.0f;
        endTime = 1000.0f;
    }

    // Re-check dirtied prims to update the animation cache
    // Handle both cases: prims that become animated and prims that are no longer animated
    for (const auto &entry : entries) {
        const HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(entry.primPath);
        if (!prim.dataSource) {
            // Prim no longer exists, remove from caches
            _animatedPrims.erase(entry.primPath);
            _primTimeVaryingDataSources.erase(entry.primPath);
            continue;
        }

        // Collect time-varying data source handles for this prim
        std::vector<HdSampledDataSourceHandle> timeVaryingDataSources;
        _CollectTimeVaryingDataSourcesRecursive(
            prim.dataSource, timeVaryingDataSources, startTime, endTime);

        const bool isCurrentlyAnimated = !timeVaryingDataSources.empty();
        const bool wasInCache = _animatedPrims.find(entry.primPath) != _animatedPrims.end();

        if (isCurrentlyAnimated && !wasInCache) {
            // Prim became animated - add to cache
            _animatedPrims.insert(entry.primPath);
            _primTimeVaryingDataSources[entry.primPath] = std::move(timeVaryingDataSources);
        } else if (!isCurrentlyAnimated && wasInCache) {
            // Prim was animated but is no longer animated - remove from cache
            _animatedPrims.erase(entry.primPath);
            _primTimeVaryingDataSources.erase(entry.primPath);
        } else if (isCurrentlyAnimated && wasInCache) {
            // Prim is still animated, update cached data sources in case they changed
            _primTimeVaryingDataSources[entry.primPath] = std::move(timeVaryingDataSources);
        }
    }

    _SendPrimsDirtied(entries);
}

} //end of namespace FVP_NS_DEF
