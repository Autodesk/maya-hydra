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

#include <mayaHydraLib/hydraUtils.h>

#include <flowViewport/selection/fvpPathMapperRegistry.h>

#include <maya/MGlobal.h>

#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

TEST(TestUsdAnim, timeVaryingTransform)
{
    const auto& si = GetTerminalSceneIndices();
    ASSERT_GT(si.size(), 0u);
    auto siRoot = si.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path cubePath(Ufe::PathString::path(argv[0]));

    // Translate the cube application path into the cube scene index path.
    const auto cubeSiPath = Fvp::sceneIndexPath(cubePath);

    ASSERT_FALSE(cubeSiPath.IsEmpty());

    // Get cube scene index prim.
    const auto cubePrim = siRoot->GetPrim(cubeSiPath);
    ASSERT_TRUE(cubePrim.dataSource);

    // Extract the Hydra xform matrix from the cube prim
    // Loop over time, and check translation, from frames [0..10].  The z value
    // of the translation has been set equal to time.
    for (double t=0; t < 11.0; t+=1.0) {
        ASSERT_EQ(MGlobal::viewFrame(t), MS::kSuccess);

        GfMatrix4d cubeHydraMatrix;
        ASSERT_TRUE(GetXformMatrixFromPrim(cubePrim, cubeHydraMatrix));
        ASSERT_TRUE(GfIsClose(cubeHydraMatrix.ExtractTranslation()[2], t, std::numeric_limits<double>::epsilon()));
    }
}
