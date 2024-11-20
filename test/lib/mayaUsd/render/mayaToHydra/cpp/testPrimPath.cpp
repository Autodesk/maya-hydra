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

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TEST(TestPrimPath, isVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);

    // We receive the Hydra prim path directly.
    const SdfPath primPath(argv[0]);

    ASSERT_TRUE(visibility(siRoot, primPath));
}

TEST(TestPrimPath, notVisible)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);

    // We receive the Hydra prim path directly.
    const SdfPath primPath(argv[0]);

    ASSERT_FALSE(visibility(siRoot, primPath));
}

TEST(TestPrimPath, exists)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);

    // We receive the Hydra prim path directly.
    const SdfPath primPath(argv[0]);

    const auto prim = siRoot->GetPrim(primPath);
    // A prim that exists has a data source.
    ASSERT_TRUE(prim.dataSource);
}

TEST(TestPrimPath, doesNotExist)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);

    // We receive the Hydra prim path directly.
    const SdfPath primPath(argv[0]);

    const auto prim = siRoot->GetPrim(primPath);
    // A prim that exists has a data source.
    ASSERT_FALSE(prim.dataSource);
}
