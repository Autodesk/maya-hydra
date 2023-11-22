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

#include <pxr/base/gf/vec3d.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

constexpr float kUnmutedExtent = 4.0f;

FindPrimPredicate findCubePredicate = PrimNamePredicate("USDCube");

HdDataSourceLocator extentMinLocator = HdDataSourceLocator(TfToken("extent"), TfToken("min"));
HdDataSourceLocator extentMaxLocator = HdDataSourceLocator(TfToken("extent"), TfToken("max"));

} // namespace

TEST(UsdStageLayerMuting, testSubLayerUnmuted)
{
    // Get the terminal scene index
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Find the cube prim
    PrimEntriesVector cubePrims = inspector.FindPrims(findCubePredicate);
    ASSERT_EQ(cubePrims.size(), 1u);
    HdSceneIndexPrim cubePrim = cubePrims.front().prim;

    // Retrieve the extent/min and extent/max data sources
    auto extentMinDataSource = HdTypedSampledDataSource<GfVec3d>::Cast(
        HdContainerDataSource::Get(cubePrim.dataSource, extentMinLocator));
    ASSERT_TRUE(extentMinDataSource);
    auto extentMaxDataSource = HdTypedSampledDataSource<GfVec3d>::Cast(
        HdContainerDataSource::Get(cubePrim.dataSource, extentMaxLocator));
    ASSERT_TRUE(extentMaxDataSource);

    // Ensure the extents have the correct values
    EXPECT_TRUE(GfIsClose(
        extentMinDataSource->GetTypedValue(0), -GfVec3d(kUnmutedExtent), DEFAULT_TOLERANCE));
    EXPECT_TRUE(GfIsClose(
        extentMaxDataSource->GetTypedValue(0), GfVec3d(kUnmutedExtent), DEFAULT_TOLERANCE));
}

TEST(UsdStageLayerMuting, testSubLayerMuted)
{
    // Get the terminal scene index
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Find the cube prim
    PrimEntriesVector cubePrims = inspector.FindPrims(findCubePredicate);
    ASSERT_EQ(cubePrims.size(), 1u);
    HdSceneIndexPrim cubePrim = cubePrims.front().prim;

    // Ensure there is no "extent" data source with the sublayer muted,
    // since the root layer did not define any special extents.
    auto extentDataSource = cubePrim.dataSource->Get(TfToken("extent"));
    EXPECT_FALSE(extentDataSource);
}
