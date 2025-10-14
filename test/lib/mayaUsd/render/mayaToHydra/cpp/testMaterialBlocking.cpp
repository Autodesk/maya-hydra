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
#include <pxr/usdImaging/usdImaging/materialBindingSchema.h>
#include <pxr/usdImaging/usdImaging/materialBindingsSchema.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(MaterialBlocking, testMaterialBlocking)
{
    // Get the selection highlight scene index
    auto secondaryGraphicsPassSceneIndex = GetSecondaryGraphicsPassSceneIndex();
    ASSERT_NE(secondaryGraphicsPassSceneIndex, nullptr);
    SceneIndexInspector inspector(secondaryGraphicsPassSceneIndex);

    // Find the cube prim
    FindPrimPredicate findCubePredicate = [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        auto pathString = primPath.GetString();
        if (pathString.find(kHighlightsHierarchyPrefix) == std::string::npos) {
            return false;
        }
        if (primPath.GetElementString() != "cubeMesh") {
            return false;
        }
        return sceneIndex->GetPrim(primPath).primType == HdPrimTypeTokens->mesh;
    };
    PrimEntriesVector cubePrims = inspector.FindPrims(findCubePredicate);
    ASSERT_EQ(cubePrims.size(), 1u);
    HdSceneIndexPrim cubePrim = cubePrims.front().prim;

    // Check the material bindings
    auto hdMaterialBinding = HdMaterialBindingsSchema::GetFromParent(cubePrim.dataSource);
    ASSERT_TRUE(hdMaterialBinding.IsDefined());
    ASSERT_FALSE(hdMaterialBinding.GetMaterialBinding().GetPath());

#if PXR_VERSION >= 2505
    auto usdMaterialBindings = UsdImagingMaterialBindingsSchema::GetFromParent(cubePrim.dataSource);
    ASSERT_TRUE(usdMaterialBindings.IsDefined());
    for (const auto& purpose : usdMaterialBindings.GetPurposes()) {
        auto materialBindings = usdMaterialBindings.GetMaterialBindings(purpose);
        for (size_t iBinding = 0; iBinding < materialBindings.GetNumElements(); iBinding++) {
            auto binding = materialBindings.GetElement(iBinding);
            ASSERT_FALSE(binding.GetDirectMaterialBinding().GetMaterialPath());
            for (size_t iCollectionBinding = 0; iCollectionBinding < binding.GetCollectionMaterialBindings().GetNumElements(); iCollectionBinding++) {
                ASSERT_FALSE(binding.GetCollectionMaterialBindings().GetElement(iCollectionBinding).GetMaterialPath());
            }
        }
    }
#else

#endif
}
