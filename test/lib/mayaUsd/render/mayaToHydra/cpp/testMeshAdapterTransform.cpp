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

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
const std::string kCubeName = "testCube";
} // namespace

TEST(MeshAdapterTransform, testDirtying)
{
    // Setup notifications accumulator for the first terminal scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), static_cast<size_t>(0));
    SceneIndexNotificationsAccumulator notifsAccumulator(sceneIndices.front());

    // The test cube should still be selected from the Python driver
    MGlobal::executeCommand("move 3 5 8");

    // Check if the cube mesh prim had its xform dirtied
    bool cubeXformWasDirtied = false;
    for (const auto& dirtiedPrimEntry : notifsAccumulator.GetDirtiedPrimEntries()) {
        HdSceneIndexPrim prim
            = notifsAccumulator.GetObservedSceneIndex()->GetPrim(dirtiedPrimEntry.primPath);

        cubeXformWasDirtied = dirtiedPrimEntry.primPath.GetName() == kCubeName + "Shape"
            && prim.primType == HdPrimTypeTokens->mesh
            && dirtiedPrimEntry.dirtyLocators.Contains(HdXformSchema::GetDefaultLocator());

        if (cubeXformWasDirtied) {
            break;
        }
    }
    EXPECT_TRUE(cubeXformWasDirtied);
}
