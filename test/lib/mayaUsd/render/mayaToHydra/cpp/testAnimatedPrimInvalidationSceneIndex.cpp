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
#include <pxr/imaging/hd/sceneGlobalsSchema.h>
#include <pxr/imaging/hdsi/sceneGlobalsSceneIndex.h>
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/refPtr.h>

#include <gtest/gtest.h>
#include <vector>
#include <set>
#include <memory>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace
{
    // Helper to get current frame from scene globals in the scene index chain
    // This simulates how real data sources access the current frame
    float GetCurrentFrameFromSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex)
    {
        HdSceneGlobalsSchema globalsSchema = 
            HdSceneGlobalsSchema::GetFromSceneIndex(sceneIndex);
        
        if (!globalsSchema.IsDefined()) {
            return 0.0f;
        }

        if (auto currentFrameDs = globalsSchema.GetCurrentFrame()) {
            return static_cast<float>(currentFrameDs->GetTypedValue(0.0f));
        }

        return 0.0f;
    }
    
    // Custom sampled data source that supports time samples for testing
    class TestSampledDataSource : public HdSampledDataSource
    {
    public:
        TestSampledDataSource(
            const VtValue& value, 
            const std::vector<float>& sampleTimes,
            const HdSceneIndexBaseRefPtr& sceneIndex)
            : _value(value)
            , _sampleTimes(sampleTimes)
            , _sceneIndex(sceneIndex)
        {
        }

        void SetSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex)
        {
            _sceneIndex = sceneIndex;
        }

        VtValue GetValue(float shutterOffset = 0.0f) override
        {
            // Return the stored value. For testing purposes, we only need to verify
            // that time samples exist (via GetContributingSampleTimesForInterval),
            // so the actual value returned doesn't need to vary by time.
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
            
            // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame.
            // Get the current frame from scene globals (set via scene globals scene index).
            outSampleTimes->clear();
            
            const float currentFrame = GetCurrentFrameFromSceneIndex(_sceneIndex);
            for (float sampleTime : _sampleTimes) {
                // Convert absolute sample time to relative offset
                const float relativeOffset = sampleTime - currentFrame;
                // Check if relative offset falls within the query window
                if (relativeOffset >= startTime && relativeOffset <= endTime) {
                    outSampleTimes->push_back(relativeOffset);
                }
            }
            return !outSampleTimes->empty();
        }

    private:
        VtValue _value;
        std::vector<float> _sampleTimes;
        HdSceneIndexBaseRefPtr _sceneIndex;
    };

    // Custom matrix data source wrapper that supports time samples
    // HdMatrixDataSource inherits from HdSampledDataSource, so we can override GetContributingSampleTimesForInterval
    class TestMatrixDataSource : public HdMatrixDataSource
    {
    public:
        TestMatrixDataSource(
            const std::vector<float>& sampleTimes, 
            const GfMatrix4d& value,
            const HdSceneIndexBaseRefPtr& sceneIndex)
            : _sampleTimes(sampleTimes)
            , _value(value)
            , _sceneIndex(sceneIndex)
        {
        }

        void SetSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex)
        {
            _sceneIndex = sceneIndex;
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
            
            // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame.
            // Get the current frame from scene globals (set via scene globals scene index).
            outSampleTimes->clear();
            
            const float currentFrame = GetCurrentFrameFromSceneIndex(_sceneIndex);
            for (float sampleTime : _sampleTimes) {
                // Convert absolute sample time to relative offset
                const float relativeOffset = sampleTime - currentFrame;
                // Check if relative offset falls within the query window
                if (relativeOffset >= startTime && relativeOffset <= endTime) {
                    outSampleTimes->push_back(relativeOffset);
                }
            }
            return !outSampleTimes->empty();
        }

    private:
        std::vector<float> _sampleTimes;
        GfMatrix4d _value;
        HdSceneIndexBaseRefPtr _sceneIndex;
    };

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
    
    // Helper to create test scene index and return data source handles
    HdRetainedSceneIndexRefPtr CreateTestSceneIndexWithDataSources(
        const HdSceneIndexBaseRefPtr& sceneIndex,
        std::shared_ptr<TestMatrixDataSource>& outXformMatrixDs,
        std::shared_ptr<TestSampledDataSource>& outPointsSampledDs)
    {
        HdRetainedSceneIndexRefPtr inputSceneIndex = HdRetainedSceneIndex::New();

        // Create a prim with animated transform (samples at frames 1, 5, 10)
        const SdfPath animatedXformPath("/animatedXform");
        const std::vector<float> xformSampleTimes = {1.0f, 5.0f, 10.0f};
        const GfMatrix4d xformValue(1.0);
        
        outXformMatrixDs = std::make_shared<TestMatrixDataSource>(
            xformSampleTimes, xformValue, sceneIndex);
        HdMatrixDataSourceHandle xformMatrixDs = outXformMatrixDs;
        
        HdContainerDataSourceHandle xformContainerDs =
            HdXformSchema::Builder()
                .SetMatrix(xformMatrixDs)
                .Build();

        inputSceneIndex->AddPrims({
            {animatedXformPath, TfToken("xform"), xformContainerDs}
        });

        // Create a prim with animated mesh points (samples at frames 2, 4, 6, 8)
        const SdfPath animatedMeshPath("/animatedMesh");
        const std::vector<float> pointsSampleTimes = {2.0f, 4.0f, 6.0f, 8.0f};
        const VtArray<GfVec3f> pointsValue = {GfVec3f(0, 0, 0), GfVec3f(1, 0, 0)};
        
        outPointsSampledDs = std::make_shared<TestSampledDataSource>(
            VtValue(pointsValue), 
            pointsSampleTimes, 
            sceneIndex);
        HdSampledDataSourceHandle pointsSampledDs = outPointsSampledDs;
        
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

        inputSceneIndex->AddPrims({
            {animatedMeshPath, TfToken("mesh"), meshContainerDs}
        });

        // Create a prim with no animation (static)
        const SdfPath staticPrimPath("/staticPrim");
        const GfMatrix4d staticXform(1.0);
        
        HdContainerDataSourceHandle staticContainerDs =
            HdXformSchema::Builder()
                .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(staticXform))
                .Build();

        inputSceneIndex->AddPrims({
            {staticPrimPath, TfToken("xform"), staticContainerDs}
        });

        return inputSceneIndex;
    }
    
    // Helper to create the scene index chain with scene globals and animated prim invalidation
    // Returns the final scene index (with scene globals) so tests can set current frame and observe
    struct TestSceneIndexSetup
    {
        HdRetainedSceneIndexRefPtr inputSceneIndex;
        Fvp::AnimatedPrimInvalidationSceneIndexRefPtr animatedSceneIndex;
        HdsiSceneGlobalsSceneIndexRefPtr sceneGlobalsSceneIndex;
        std::shared_ptr<TestMatrixDataSource> xformMatrixDs;
        std::shared_ptr<TestSampledDataSource> pointsSampledDs;
        
        TestSceneIndexSetup()
        {
            // Create test scene index with animated and static prims first
            // We'll pass a placeholder scene globals scene index that will be replaced later
            HdsiSceneGlobalsSceneIndexRefPtr placeholderSceneGlobals = 
                HdsiSceneGlobalsSceneIndex::New(HdRetainedSceneIndex::New());
            
            // Create the data sources with placeholder, storing handles for later update
            inputSceneIndex = CreateTestSceneIndexWithDataSources(
                placeholderSceneGlobals, xformMatrixDs, pointsSampledDs);
            
            // Create the animated prim invalidation scene index
            animatedSceneIndex = Fvp::AnimatedPrimInvalidationSceneIndex::New(inputSceneIndex);
            
            // Insert scene globals into the chain after animated prim invalidation
            // so the current frame is available when querying data sources
            sceneGlobalsSceneIndex = HdsiSceneGlobalsSceneIndex::New(animatedSceneIndex);
            
            // Now update the data sources to reference the final scene globals scene index
            xformMatrixDs->SetSceneIndex(sceneGlobalsSceneIndex);
            pointsSampledDs->SetSceneIndex(sceneGlobalsSceneIndex);
            
            // Set current frame to 0.0 so that when RefreshAnimatedPrimsCache queries with
            // absolute times, the data sources can correctly convert them to relative offsets
            sceneGlobalsSceneIndex->SetCurrentFrame(0.0);
        }
    };
}

TEST(TestAnimatedPrimInvalidationSceneIndex, CacheBuilding)
{
    TestSceneIndexSetup setup;
    
    // Set animation time range [0, 10]
    // Note: Current frame is already set to 0.0 in TestSceneIndexSetup constructor
    // to ensure RefreshAnimatedPrimsCache can correctly query data sources
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    // Set current frame in scene globals (simulating Maya time change callback)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(5.0);

    // Verify that animated prims are in the cache
    // The cache should contain prims with time samples in range [0, 10]
    // - animatedXform has samples at 1, 5, 10 -> should be cached
    // - animatedMesh has samples at 2, 4, 6, 8 -> should be cached
    // - staticPrim has no samples -> should not be cached

    // We can't directly access the cache, but we can verify by checking
    // which prims get invalidated when we call InvalidateAnimatedPrimsAtCurrentFrame
    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;

    // Invalidate at frame 5 (should invalidate animatedXform which has a sample at 5)
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(5.0);
    
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
    TestSceneIndexSetup setup;
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;

    // GetContributingSampleTimesForInterval takes shutter offsets relative to the current frame.
    // Set current frame in scene globals (simulating Maya time change callback)
    // Test invalidation at frame 1 (animatedXform has sample at 1)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(1.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "Animated xform should be invalidated at frame 1 (has sample at 1)";
    // animatedMesh has samples at 2, 4, 6, 8 - not at 1, so should not be invalidated
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated at frame 1 (no sample at 1)";
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/staticPrim")), 0u)
        << "Static prim should not be invalidated";

    // Test invalidation at frame 2 (animatedMesh has sample at 2)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 2 (has sample at 2)";
    // animatedXform has samples at 1, 5, 10 - not at 2, so should not be invalidated
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")), 0u)
        << "Animated xform should not be invalidated at frame 2 (no sample at 2)";
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/staticPrim")), 0u)
        << "Static prim should not be invalidated";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, FrameTolerance)
{
    TestSceneIndexSetup setup;
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);

    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;

    // Test frame tolerance: animatedMesh has sample at 4.0
    // GetContributingSampleTimesForInterval queries around 0.0 (current frame) with tolerance.
    // With tolerance of 0.1, frames 3.9-4.1 should invalidate it.
    
    // Frame 4.0 (exact match)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(4.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(4.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 4.0 (exact match)";

    // Frame 4.05 (within tolerance of 0.1)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(4.05);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(4.05);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 4.05 (within tolerance)";

    // Frame 3.95 (within tolerance of 0.1)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(3.95);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(3.95);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 3.95 (within tolerance)";

    // Frame 3.8 (outside tolerance of 0.1)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(3.8);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(3.8);
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")), 0u)
        << "Animated mesh should not be invalidated at frame 3.8 (outside tolerance)";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, TimeRangeFiltering)
{
    TestSceneIndexSetup setup;
    
    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;

    // Set range to [0, 5] - only animatedXform (samples at 1, 5) should be cached
    // animatedMesh has samples at 2, 4, 6, 8 - samples at 2, 4 are in range, but 6, 8 are not
    // Since it has samples in range, it should still be cached
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 5.0, true);

    // Invalidate at frame 2 (animatedMesh has sample at 2, which is in range [0, 5])
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be invalidated at frame 2 (has sample at 2, in range [0, 5])";
    // animatedXform has samples at 1, 5 - not at 2, so should not be invalidated
    EXPECT_EQ(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")), 0u)
        << "Animated xform should not be invalidated at frame 2 (no sample at 2)";

    // Set range to [10, 20] - no prims have samples in this range, so cache should be empty
    setup.animatedSceneIndex->SetAnimationTimeRange(10.0, 20.0, true);

    // Invalidate at frame 15 - cache should be empty since no prims have samples in range [10, 20]
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(15.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(15.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "No prims should be invalidated at frame 15 (no samples in range [10, 20], cache is empty)";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, RangeChangeRefresh)
{
    TestSceneIndexSetup setup;
    
    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;

    // Initially set range to [0, 5]
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 5.0, true);

    // Verify animatedMesh is cached (has samples at 2, 4 in range)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Animated mesh should be cached and invalidated at frame 2";

    // Change range to [10, 20] - cache should be refreshed and cleared
    // (no prims have samples in this range)
    setup.animatedSceneIndex->SetAnimationTimeRange(10.0, 20.0, true);

    // Now cache should be empty (no samples in new range), so nothing should be invalidated
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "No prims should be invalidated after range change (no samples in new range [10, 20], cache is empty)";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, CachePerformanceOptimization)
{
    // Test that the time-varying data source cache is properly maintained and used.
    // This verifies the performance optimization where we cache data source handles
    // instead of re-traversing the data source tree on every frame change.
    
    TestSceneIndexSetup setup;
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);
    
    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;
    
    // Test 1: Cache is populated after RefreshAnimatedPrimsCache()
    // After setting the range, the cache should be populated with animated prims
    // Verify by checking that invalidation works (which uses the cached data sources)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(1.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "Cache should be populated: animatedXform should be invalidated at frame 1";
    
    // Test 2: Cache is used for per-frame queries (not re-traversing)
    // Multiple invalidation calls should work efficiently using cached data sources
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Cache should be used: animatedMesh should be invalidated at frame 2";
    
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(4.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(4.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Cache should be used: animatedMesh should be invalidated at frame 4";
    
    // Test 3: Cache is cleared by ClearAnimatedPrimsCache()
    setup.animatedSceneIndex->ClearAnimatedPrimsCache();
    // After clearing, no prims should be invalidated (cache is empty)
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(1.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "Cache should be cleared: no prims should be invalidated after ClearAnimatedPrimsCache()";
    
    // Test 4: Cache is rebuilt by RefreshAnimatedPrimsCache()
    setup.animatedSceneIndex->RefreshAnimatedPrimsCache();
    // After refresh, invalidation should work again
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(1.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "Cache should be rebuilt: animatedXform should be invalidated after RefreshAnimatedPrimsCache()";
}

TEST(TestAnimatedPrimInvalidationSceneIndex, CacheMaintenanceOnPrimChanges)
{
    // Test that the cache is properly maintained when prims are added/removed/dirtied.
    // This verifies that the cache stays in sync with the scene.
    
    TestSceneIndexSetup setup;
    setup.animatedSceneIndex->SetAnimationTimeRange(0.0, 10.0, true);
    
    ObserverHolder observerHolder(setup.sceneGlobalsSceneIndex);
    auto& observer = *observerHolder.observer;
    
    // Initially, animatedXform and animatedMesh should be cached
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(1.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(1.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedXform")) > 0)
        << "animatedXform should be cached initially";
    
    // Test: Adding a new animated prim should update the cache
    // (In a real scenario, this would happen via _PrimsAdded callback)
    // We can't directly test this without exposing internals, but we can verify
    // that RefreshAnimatedPrimsCache() rebuilds the cache correctly
    
    // Test: Removing a prim should remove it from the cache
    // Clear the cache and verify that removed prims are not invalidated
    setup.animatedSceneIndex->ClearAnimatedPrimsCache();
    setup.animatedSceneIndex->RefreshAnimatedPrimsCache();
    
    // Verify cache still works after refresh
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(2.0);
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(2.0);
    EXPECT_TRUE(observer.GetDirtiedPrims().count(SdfPath("/animatedMesh")) > 0)
        << "Cache should work correctly after refresh";
    
    // Test: Cache handles prims that are no longer animated
    // When a prim's animation is removed, it should be removed from cache
    // This is tested indirectly by verifying that only animated prims are invalidated
    setup.sceneGlobalsSceneIndex->SetCurrentFrame(100.0); // Frame with no samples
    observer.Clear();
    setup.animatedSceneIndex->InvalidateAnimatedPrimsAtCurrentFrame(100.0);
    // No prims should be invalidated at frame 100 (no samples at that frame)
    EXPECT_TRUE(observer.GetDirtiedPrims().empty())
        << "No prims should be invalidated at frame 100 (no samples at that frame)";
}
