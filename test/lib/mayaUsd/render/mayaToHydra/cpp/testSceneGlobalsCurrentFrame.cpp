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

    // Test that changing Maya's frame automatically updates the scene globals scene index
    // via the timeChanged callback registered in renderOverride
    const double testFrames[] = {0.0, 5.0, 10.0, 15.0, 20.0, 25.0, 0.0};

    for (double testFrame : testFrames) {
        // Change Maya's current frame - this should trigger the timeChanged callback
        // which calls _SetCurrentFrameInHydraGlobalSceneIndex()
        ASSERT_EQ(MGlobal::viewFrame(testFrame), MS::kSuccess);

        // Verify Maya's frame was set correctly
        const MTime mayaTime = MAnimControl::currentTime();
        const double mayaFrame = mayaTime.value();
        EXPECT_NEAR(testFrame, mayaFrame, std::numeric_limits<double>::epsilon())
            << "Maya frame mismatch: expected " << testFrame << ", got " << mayaFrame;

        // Force a refresh to ensure the callback has been processed
        // The callback should fire synchronously, but we refresh to ensure Hydra processes it
        MGlobal::executeCommand("refresh", false, true);

        // Get the current frame from the scene globals schema
        // This should match Maya's frame if the callback worked correctly
        double hydraFrame = GetCurrentFrameFromSceneIndex(siRoot);
        
        // Verify the frame in Hydra matches Maya's frame
        EXPECT_NEAR(mayaFrame, hydraFrame, std::numeric_limits<double>::epsilon())
            << "Hydra frame (" << hydraFrame << ") does not match Maya frame (" << mayaFrame 
            << ") after changing to frame " << testFrame << ". The timeChanged callback may not be working correctly.";
    }
}
