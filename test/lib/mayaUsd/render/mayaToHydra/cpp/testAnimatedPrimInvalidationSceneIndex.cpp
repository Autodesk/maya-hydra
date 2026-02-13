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

#include "testUtils.h"

#include <flowViewport/sceneIndex/fvpAnimatedPrimInvalidationSceneIndex.h>

#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/refPtr.h>

#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <memory>
#include <cmath>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace
{
    // Custom sampled data source that supports time samples for testing
    class TestSampledDataSource : public HdSampledDataSource
    {
    public:
        TestSampledDataSource(const VtValue& value, const std::vector<float>& sampleTimes)
            : _value(value)
            , _sampleTimes(sampleTimes)
        {
        }

        VtValue GetValue(float shutterOffset = 0.0f) override
        {
            // Return the value at the closest sample time
            if (_sampleTimes.empty()) {
                return _value;
            }
            
            // Find closest sample time
            float closestTime = _sampleTimes[0];
            float minDiff = std::abs(shutterOffset - closestTime);
            for (float sampleTime : _sampleTimes) {
                const float diff = std::abs(shutterOffset - sampleTime);
                if (diff < minDiff) {
                    minDiff = diff;
                    closestTime = sampleTime;
                }
            }
            return _value;
        }

        bool GetContributingSampleTimesForInterval(
            float startTime,
            float endTime,
            std::vector<float>* outSampleTimes) override
        {
            if (!outSampleTimes) {
                return false;
            }
            
            outSampleTimes->clear();
            for (float sampleTime : _sampleTimes) {
                if (sampleTime >= startTime && sampleTime <= endTime) {
                    outSampleTimes->push_back(sampleTime);
                }
            }
            return !outSampleTimes->empty();
        }

    private:
        VtValue _value;
        std::vector<float> _sampleTimes;
    };

    // Custom matrix data source wrapper that supports time samples
    // HdMatrixDataSource inherits from HdSampledDataSource, so we can override GetContributingSampleTimesForInterval
    class TestMatrixDataSource : public HdMatrixDataSource
    {
    public:
        TestMatrixDataSource(const std::vector<float>& sampleTimes, const GfMatrix4d& value)
            : _sampleTimes(sampleTimes)
            , _value(value)
        {
        }

        VtValue GetValue(float shutterOffset = 0.0f) override
        {
            return VtValue(_value);
        }

        GfMatrix4d GetTypedValue(float shutterOffset = 0.0f) override
        {
            return _value;
        }

        bool GetContributingSampleTimesForInterval(
            float startTime,
            float endTime,
            std::vector<float>* outSampleTimes) override
        {
            if (!outSampleTimes) {
                return false;
            }
            
            outSampleTimes->clear();
            for (float sampleTime : _sampleTimes) {
                if (sampleTime >= startTime && sampleTime <= endTime) {
                    outSampleTimes->push_back(sampleTime);
                }
            }
            return !outSampleTimes->empty();
        }

    private:
        std::vector<float> _sampleTimes;
        GfMatrix4d _value;
    };

    // Helper to create a retained scene index with test prims
    HdRetainedSceneIndexRefPtr CreateTestSceneIndex()
    {
        HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();

        // Create a prim with animated transform (samples at frames 1, 5, 10)
        const SdfPath animatedXformPath("/animatedXform");
        const std::vector<float> xformSampleTimes = {1.0f, 5.0f, 10.0f};
        const GfMatrix4d xformValue(1.0);
        
        HdMatrixDataSourceHandle xformMatrixDs = 
            std::make_shared<TestMatrixDataSource>(xformSampleTimes, xformValue);
        
        HdContainerDataSourceHandle xformContainerDs =
            HdXformSchema::Builder()
                .SetMatrix(xformMatrixDs)
                .Build();

        sceneIndex->AddPrims({
            {animatedXformPath, TfToken("xform"), xformContainerDs}
        });

        // Create a prim with animated mesh points (samples at frames 2, 4, 6, 8)
        const SdfPath animatedMeshPath("/animatedMesh");
        const std::vector<float> pointsSampleTimes = {2.0f, 4.0f, 6.0f, 8.0f};
        const VtArray<GfVec3f> pointsValue = {GfVec3f(0, 0, 0), GfVec3f(1, 0, 0)};
        
        HdSampledDataSourceHandle pointsSampledDs = 
            std::make_shared<TestSampledDataSource>(VtValue(pointsValue), pointsSampleTimes);
        
        // Create mesh with animated points in primvars
        HdContainerDataSourceHandle meshContainerDs =
            HdRetainedContainerDataSource::New(
                HdMeshSchemaTokens->mesh,
                HdMeshSchema::Builder()
                    .SetTopology(HdMeshTopologySchema::Builder()
                        .SetFaceVertexCounts(HdRetainedTypedSampledDataSource<VtIntArray>::New(VtIntArray{2}))
                        .SetFaceVertexIndices(HdRetainedTypedSampledDataSource<VtIntArray>::New(VtIntArray{0, 1}))
                        .Build())
                    .Build(),
                HdPrimvarsSchemaTokens->primvars,
                HdRetainedContainerDataSource::New(
                    HdTokens->points,
                    HdPrimvarSchema::Builder()
                        .SetPrimvarValue(pointsSampledDs)
                        .SetInterpolation(HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
                        .SetRole(HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->point))
                        .Build())
            );

        sceneIndex->AddPrims({
            {animatedMeshPath, TfToken("mesh"), meshContainerDs}
        });

        // Create a prim with no animation (static)
        const SdfPath staticPrimPath("/staticPrim");
        const GfMatrix4d staticXform(1.0);
        
        HdContainerDataSourceHandle staticContainerDs =
            HdXformSchema::Builder()
                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(staticXform))
                .Build();

        sceneIndex->AddPrims({
            {staticPrimPath, TfToken("xform"), staticContainerDs}
        });

        return sceneIndex;
    }

    // Observer to track dirtied prims
    class TestObserver : public HdSceneIndexObserver, public TfRefBase
    {
    public:
        void PrimsAdded(
            const HdSceneIndexBase& sender,
            const AddedPrimEntries& entries) override
        {
            // Not used in this test
        }

        void PrimsRemoved(
            const HdSceneIndexBase& sender,
            const RemovedPrimEntries& entries) override
        {
            // Not used in this test
        }

        void PrimsRenamed(
            const HdSceneIndexBase& sender,
            const RenamedPrimEntries& entries) override
        {
            // Not used in this test
        }

        void PrimsDirtied(
            const HdSceneIndexBase& sender,
            const DirtiedPrimEntries& entries) override
        {
            for (const auto& entry : entries) {
                _dirtiedPrims.insert(entry.primPath);
            }
        }

        void Clear()
        {
            _dirtiedPrims.clear();
        }

        const std::set<SdfPath>& GetDirtiedPrims() const
        {
            return _dirtiedPrims;
        }

    private:
        std::set<SdfPath> _dirtiedPrims;
    };

    TF_DECLARE_REF_PTRS(TestObserver);

    // Helper to create and manage observer with proper lifetime
    struct ObserverHolder
    {
        ObserverHolder(HdSceneIndexBaseRefPtr sceneIndex)
            : observer(TfCreateRefPtr(new TestObserver()))
        {
            // Get raw pointer and convert to HdSceneIndexObserverPtr
            sceneIndex->AddObserver(HdSceneIndexObserverPtr(observer.operator->()));
        }

        TestObserverRefPtr observer;
    };
}

TEST(TestAnimatedPrimInvalidationSceneIndex, CacheBuilding)
{
    // Create test scene index with animated and static prims
    HdRetainedSceneIndexRefPtr inputSceneIndex = CreateTestSceneIndex();
    
    // Create the animated prim invalidation scene index
    Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex =
        Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);

    // Set animation time range [0, 10]
    animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    // Verify that animated prims are in the cache
    // The cache should contain prims with time samples in range [0, 10]
    // - animatedXform has samples at 1, 5, 10 -> should be cached
    // - animatedMesh has samples at 2, 4, 6, 8 -> should be cached
    // - staticPrim has no samples -> should not be cached

    // We can't directly access the cache, but we can verify by checking
    // which prims get invalidated when we call InvalidateAnimatedPrimsAtCurrentFrame
    ObserverHolder observerHolder(animatedSceneIndex);
    auto& observer = *observerHolder.observer;

    // Invalidate at frame 5 (should invalidate animatedXform which has a sample at 5)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(5.0);
    
    // animatedXform should be dirtied (has sample at frame 5, exact match)
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "Animated xform should be invalidated at frame 5";
    
    // animatedMesh should NOT be dirtied (has sample at frame 4, which is 1.0 frames away - outside 0.1 tolerance)
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated at frame 5 (sample at 4 is outside 0.1 frame tolerance)";
    
    // staticPrim should not be dirtied
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/staticPrim")), 0u)
        << "Static prim should not be invalidated";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, PerFrameInvalidation)
{
    HdRetainedSceneIndexRefPtr inputSceneIndex = CreateTestSceneIndex();
    Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex =
        Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);

    animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    ObserverHolder observerHolder(animatedSceneIndex);
    auto& observer = *observerHolder.observer;

    // Test invalidation at frame 1 (animatedXform has sample at 1)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "Animated xform should be invalidated at frame 1";
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated at frame 1 (no sample nearby)";

    // Test invalidation at frame 2 (animatedMesh has sample at 2)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 2";
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")), 0u)
        << "Animated xform should not be invalidated at frame 2 (no sample nearby)";

    // Test invalidation at frame 0 (no samples, should invalidate nothing)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(0.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "No prims should be invalidated at frame 0 (no samples)";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, FrameTolerance)
{
    HdRetainedSceneIndexRefPtr inputSceneIndex = CreateTestSceneIndex();
    Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex =
        Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);

    animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    ObserverHolder observerHolder(animatedSceneIndex);
    auto& observer = *observerHolder.observer;

    // Test frame tolerance: animatedMesh has sample at 4.0
    // With tolerance of 0.1, frames 3.9-4.1 should invalidate it
    
    // Frame 4.0 (exact match)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(4.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 4.0 (exact match)";

    // Frame 4.05 (within tolerance of 0.1)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(4.05);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 4.05 (within tolerance)";

    // Frame 3.95 (within tolerance of 0.1)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(3.95);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 3.95 (within tolerance)";

    // Frame 3.8 (outside tolerance of 0.1)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(3.8);
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated at frame 3.8 (outside tolerance)";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, TimeRangeFiltering)
{
    HdRetainedSceneIndexRefPtr inputSceneIndex = CreateTestSceneIndex();
    Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex =
        Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);

    ObserverHolder observerHolder(animatedSceneIndex);
    auto& observer = *observerHolder.observer;

    // Set range to [0, 5] - only animatedXform (samples at 1, 5) should be cached
    // animatedMesh has samples at 2, 4, 6, 8 - samples at 2, 4 are in range, but 6, 8 are not
    // Since it has samples in range, it should still be cached
    animatedSceneIndex->SetAnimationTimeRange(0.0, 5.0, true);

    // Invalidate at frame 2 (animatedMesh has sample at 2, which is in range)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 2 (sample is in range [0, 5])";

    // Set range to [10, 20] - no prims have samples in this range
    animatedSceneIndex->SetAnimationTimeRange(10.0, 20.0, true);

    // Invalidate at frame 15 (no samples in range)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(15.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "No prims should be invalidated at frame 15 (no samples in range [10, 20])";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, RangeChangeRefresh)
{
    HdRetainedSceneIndexRefPtr inputSceneIndex = CreateTestSceneIndex();
    Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex =
        Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);

    ObserverHolder observerHolder(animatedSceneIndex);
    auto& observer = *observerHolder.observer;

    // Initially set range to [0, 5]
    animatedSceneIndex->SetAnimationTimeRange(0.0, 5.0, true);

    // Verify animatedMesh is cached (has samples at 2, 4 in range)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be cached and invalidated at frame 2";

    // Change range to [10, 20] - cache should be refreshed
    animatedSceneIndex->SetAnimationTimeRange(10.0, 20.0, true);

    // Now animatedMesh should not be in cache (no samples in new range)
    observer.Clear();
    animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated after range change (no samples in new range)";
}
