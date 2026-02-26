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

#include <pxr/imaging/hd/sceneGlobalsSchema.h>

#include <maya/MGlobal.h>
#include <maya/MAnimControl.h>

#include <gtest/gtest.h>
#include <limits>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace
{
    // Helper function to get current frame from scene globals schema
    // This reads the frame that was set by the renderOverride callback
    double GetCurrentFrameFromSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex)
    {
        HdSceneGlobalsSchema globalsSchema = 
            HdSceneGlobalsSchema::GetFromSceneIndex(sceneIndex);
        
        if (!globalsSchema.IsDefined()) {
            return std::numeric_limits<double>::quiet_NaN();
        }

        if (auto currentFrameDs = globalsSchema.GetCurrentFrame()) {
            return currentFrameDs->GetTypedValue(0.0f);
        }

        return std::numeric_limits<double>::quiet_NaN();
    }
}

TEST(TestSceneGlobalsCurrentFrame, SyncWithMayaTime)
{
    // Get the terminal scene indices from Maya Hydra (with Storm renderer)
    // This assumes the test is run with Storm as the active renderer
    const auto& si = GetTerminalSceneIndices();
    ASSERT_GT(si.size(), 0u) << "No terminal scene indices found. Make sure Maya Hydra with Storm is active.";
    auto siRoot = si.front();

    // Set up an observer to track dirty notifications
    // This will verify that SetCurrentFrame sends dirty notifications
    SceneIndexNotificationsAccumulator observer(siRoot);

    // Use a practical tolerance for comparing Maya time / Hydra values.
    // This accounts for unit conversion and float/double rounding errors.
    // Using 1e-3 frames (0.001) as a reasonable tolerance for frame comparisons.
    constexpr double frameTolerance = 1e-3;

    // Test that changing Maya's frame automatically updates the scene globals scene index
    // via the timeChanged callback registered in renderOverride
    constexpr double testFrames[] = {0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 0.0};

    // Get the expected scene globals path and locator for dirty notifications
    const SdfPath sceneGlobalsPath = HdSceneGlobalsSchema::GetDefaultPrimPath();
    const HdDataSourceLocator currentFrameLocator = HdSceneGlobalsSchema::GetCurrentFrameLocator();

    for (double testFrame : testFrames) {
        // Track the number of dirty notifications before changing frame
        const size_t dirtiedCountBefore = observer.GetDirtiedPrimEntries().size();
        
        // Change Maya's current frame - this should trigger the timeChanged callback
        // which calls _SetCurrentFrameInHydraGlobalSceneIndex()
        ASSERT_EQ(MGlobal::viewFrame(testFrame), MS::kSuccess);

        // Verify Maya's frame was set correctly
        const MTime mayaTime = MAnimControl::currentTime();
        const double mayaFrame = mayaTime.value();
        EXPECT_NEAR(testFrame, mayaFrame, frameTolerance)
            << "Maya frame mismatch: expected " << testFrame << ", got " << mayaFrame;

        // Force a refresh to ensure the callback has been processed
        // The callback should fire synchronously, but we refresh to ensure Hydra processes it
        MGlobal::executeCommand("refresh", false, true);

        // Get the current frame from the scene globals schema
        // This should match Maya's frame if the callback worked correctly
        const double hydraFrame = GetCurrentFrameFromSceneIndex(siRoot);
        
        // Verify the frame in Hydra matches Maya's frame
        EXPECT_NEAR(mayaFrame, hydraFrame, frameTolerance)
            << "Hydra frame (" << hydraFrame << ") does not match Maya frame (" << mayaFrame 
            << ") after changing to frame " << testFrame << ". The timeChanged callback may not be working correctly.";

        // Verify that we received a dirty notification for the scene globals prim
        // When SetCurrentFrame is called on the scene globals scene index, it should send:
        // _SendPrimsDirtied({{HdSceneGlobalsSchema::GetDefaultPrimPath(), HdSceneGlobalsSchema::GetCurrentFrameLocator()}})
        const auto& dirtiedEntries = observer.GetDirtiedPrimEntries();
        
        // Check if we received new dirty notifications
        EXPECT_GT(dirtiedEntries.size(), dirtiedCountBefore)
            << "Expected new dirty notifications after changing to frame " << testFrame 
            << ". SetCurrentFrame should send dirty notifications.";
        
        // Only check newly-added dirty entries since dirtiedCountBefore to verify per-frame behavior
        // This ensures we're checking the dirty notification for this specific frame change,
        // not accumulated entries from previous iterations
        bool foundSceneGlobalsDirty = false;
        for (size_t i = dirtiedCountBefore; i < dirtiedEntries.size(); ++i) {
            const auto& dirtiedEntry = dirtiedEntries[i];
            if (dirtiedEntry.primPath == sceneGlobalsPath) {
                // Check if the currentFrame locator is in the dirty locators
                if (dirtiedEntry.dirtyLocators.Contains(currentFrameLocator)) {
                    foundSceneGlobalsDirty = true;
                    break;
                }
            }
        }
        
        EXPECT_TRUE(foundSceneGlobalsDirty)
            << "Expected dirty notification for scene globals prim (" << sceneGlobalsPath 
            << ") with currentFrame locator (" << currentFrameLocator << ") after changing to frame " 
            << testFrame << ". SetCurrentFrame should send a dirty notification.";
    }
}
