// Copyright 2025 Autodesk
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
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/usdImaging/usdImaging/collectionMaterialBindingSchema.h>

#if PXR_VERSION >= 2505
#include <pxr/usdImaging/usdImaging/materialBindingSchema.h>
#include <pxr/usdImaging/usdImaging/materialBindingsSchema.h>
#endif

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(HydraGenerativeProcedural, testMaterialBinding)
{
    // Get the beauty pass scene index
    auto beautyPassSceneIndex = GetBeautyPassSceneIndex();
    ASSERT_NE(beautyPassSceneIndex, nullptr);
    SceneIndexInspector inspector(beautyPassSceneIndex);

    // Find the cube prim
    FindPrimPredicate findCubePredicate = [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        const std::string name = primPath.GetElementString();
        if (name != "cube0" && name != "cube1") {
            return false;
        }
        return sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
    };
    PrimEntriesVector cubePrims = inspector.FindPrims(findCubePredicate);
    ASSERT_GE(cubePrims.size(), 1u);  // at least one cube (cube0 or cube1)
    HdSceneIndexPrim cubePrim = cubePrims.front().prim;

    // Check the material bindings
    auto hdMaterialBinding = HdMaterialBindingsSchema::GetFromParent(cubePrim.dataSource);
    ASSERT_TRUE(hdMaterialBinding.IsDefined());
    HdPathDataSourceHandle pathDs = hdMaterialBinding.GetMaterialBinding().GetPath();
    ASSERT_TRUE(pathDs);
    SdfPath materialPath = pathDs->GetTypedValue(0.0f);
    ASSERT_FALSE(materialPath.IsEmpty());

    // Check the material primType
    HdSceneIndexPrim materialPrim = beautyPassSceneIndex->GetPrim(materialPath);
    ASSERT_EQ(materialPrim.primType, HdPrimTypeTokens->material);
}
