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
#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
FindPrimPredicate findRootPredicate = PrimNamePredicate("root");
HdDataSourceLocator instancerLocator = HdDataSourceLocator(TfToken("instance"), TfToken("instancer"));
} // namespace

TEST(PrimInstancing, testUsdPrimInstancing)
{
    // Get the terminal scene index
    const auto& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Find the root prim
    PrimEntriesVector rootPrims = inspector.FindPrims(findRootPredicate);
    ASSERT_EQ(rootPrims.size(), 1u);
    HdSceneIndexPrim rootPrim = rootPrims.front().prim;

    // Retrieve the instancer data source
    auto instancerDataSource = HdTypedSampledDataSource<SdfPath>::Cast(
        HdContainerDataSource::Get(rootPrim.dataSource, instancerLocator));
    ASSERT_TRUE(instancerDataSource);

    // Ensure the instancer prim exists and is populated
    SdfPath instancerPath = instancerDataSource->GetTypedValue(0);
    auto findInstancerPredicate
        = [instancerPath](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        return primPath == instancerPath;
    };
    PrimEntriesVector instancerPrims = inspector.FindPrims(findInstancerPredicate);
    ASSERT_EQ(instancerPrims.size(), 1u);
    HdSceneIndexPrim instancerPrim = instancerPrims.front().prim;
    ASSERT_EQ(instancerPrim.primType, HdPrimTypeTokens->instancer);
    ASSERT_NE(instancerPrim.dataSource, nullptr);

    // Ensure the cube prim exists and is populated
    auto findCubePredicate
        = [instancerPath](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        return primPath.HasPrefix(instancerPath) && primPath.GetName() == "cubeMesh";
    };
    PrimEntriesVector cubePrims = inspector.FindPrims(findCubePredicate);
    ASSERT_EQ(cubePrims.size(), 1u);
    HdSceneIndexPrim cubePrim = cubePrims.front().prim;
    ASSERT_EQ(cubePrim.primType, HdPrimTypeTokens->mesh);
    ASSERT_NE(cubePrim.dataSource, nullptr);
}
