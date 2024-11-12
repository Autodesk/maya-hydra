// Copyright 2023 Autodesk
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

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(TestUsdStageInvalidate, testAddStage)
{
    // Setup notifications accumulator for the first terminal scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto siRoot = sceneIndices.front();

    // Start accumulating notifications.
    SceneIndexNotificationsAccumulator notifsAccumulator(siRoot);

    // Add a stage with new layer.
    MGlobal::executePythonCommand("import mayaUsd_createStageWithNewLayer; mayaUsd_createStageWithNewLayer.createStageWithNewLayer()");

    // Confirm the new stage is in the Hydra scene.
    auto prim = siRoot->GetPrim(SdfPath("/MayaUsdProxyShape_PluginNode/mayaUsdProxyShape1"));
    ASSERT_TRUE(prim.dataSource);

    // There should not have been any prim removed notifications, only prim
    // added and dirtied.
    ASSERT_EQ(notifsAccumulator.GetRemovedPrimEntries().size(), 0u);
    ASSERT_GT(notifsAccumulator.GetDirtiedPrimEntries().size(), 0u);
    ASSERT_GT(notifsAccumulator.GetAddedPrimEntries().size(), 0u);
}
