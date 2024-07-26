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

#include <ufe/path.h>
#include <ufe/pathString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

SdfPath fromAppPath(const Ufe::Path& appPath)
{
    auto siRoot = GetTerminalSceneIndices().front();

    // Translate the application path into a scene index path using the
    // selection scene index.
    const auto snSi = findSelectionSceneIndexInTree(siRoot);
    return snSi->SceneIndexPath(appPath);
}

}

TEST(TestHydraPrim, fromAppPath)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    // Translate the application path into a scene index path using the
    // selection scene index.
    const auto snSi = findSelectionSceneIndexInTree(siRoot);
    ASSERT_TRUE(snSi);

    const auto sceneIndexPath = snSi->SceneIndexPath(appPath);

    ASSERT_FALSE(sceneIndexPath.IsEmpty());
}

TEST(TestHydraPrim, isFound)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    const auto sceneIndexPath = fromAppPath(appPath);

    ASSERT_TRUE(siRoot->GetPrim(sceneIndexPath).dataSource);
}

TEST(TestHydraPrim, isNotFound)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 1);
    const Ufe::Path appPath(Ufe::PathString::path(argv[0]));

    const auto sceneIndexPath = fromAppPath(appPath);

    ASSERT_FALSE(siRoot->GetPrim(sceneIndexPath).dataSource);
}

TEST(TestHydraPrim, translation)
{
    const auto& sceneIndices = GetTerminalSceneIndices();
    auto siRoot = sceneIndices.front();

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 4);
    const Ufe::Path appPath{Ufe::PathString::path(argv[0])};
    const GfVec3d expectedTranslation(
        std::stod(argv[1]), std::stod(argv[2]), std::stod(argv[3]));

    const auto sceneIndexPath = fromAppPath(appPath);
    const auto prim = siRoot->GetPrim(sceneIndexPath);
    GfMatrix4d m;
    ASSERT_TRUE(MayaHydra::GetXformMatrixFromPrim(prim, m));
    const auto primTranslation = m.ExtractTranslation();

    constexpr double epsilon{1e-7};
    ASSERT_TRUE(GfIsClose(primTranslation, expectedTranslation, epsilon));
}
