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

#include "testUtils.h"

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(SceneIndexDirtying, testDirtyingNew)
{
    // Setup notifications accumulator for the first terminal scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexNotificationsAccumulator notifsAccumulator(sceneIndices.front());
    
    // Set wireframeOnShaded mode
    MString wireframeOnCmd = "select -cl; modelEditor -e -wireframeOnShaded 1 modelPanel4; refresh;";

    MGlobal::executeCommand(wireframeOnCmd);

    const auto& dirtiedPrimEntries = notifsAccumulator.GetDirtiedPrimEntries();
    const auto& addedPrimEntries = notifsAccumulator.GetAddedPrimEntries();
    const auto& removedPrimEntries = notifsAccumulator.GetRemovedPrimEntries();

    EXPECT_TRUE(dirtiedPrimEntries.size());  // Expect non-zero dirtied prims.
    EXPECT_FALSE(addedPrimEntries.size());   // Changing Hydra reprs(via reprSelectorSceneIndex) 
    EXPECT_FALSE(removedPrimEntries.size()); // should not cause prim Addition or removal.
}
