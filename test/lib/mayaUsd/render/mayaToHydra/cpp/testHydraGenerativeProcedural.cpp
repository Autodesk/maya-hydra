// Copyright 2026 Autodesk
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

#include <maya/M3dView.h>
#include <maya/MPoint.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

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

TEST(HydraGenerativeProcedural, testPickGeneratedChild)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    auto [argc, argv] = getTestingArgs();
    ASSERT_EQ(argc, 3);
    const std::string childName(argv[0]); 
    const TfToken     childType(argv[1]);
    const std::string proceduralName(argv[2]);

    // Find the generated child to get click coordinates
    FindPrimPredicate findChild = [&](const HdSceneIndexBaseRefPtr& si, const SdfPath& path) {
        return path.GetElementString() == childName && si->GetPrim(path).primType == childType;
    };
    PrimEntriesVector childPrims = inspector.FindPrims(findChild);
    ASSERT_GE(childPrims.size(), 1u);

    ensureUnselected(inspector, PrimNamePredicate(proceduralName));

    // Click on the child mesh
    M3dView active3dView = M3dView::active3dView();
    auto    childCoords = getPrimMouseCoords(childPrims.front().prim, active3dView);
    mouseClick(Qt::MouseButton::LeftButton, active3dView.widget(), childCoords);
    active3dView.refresh();

    // Verify the procedural parent is selected, not the child directly.
    ensureSelected(inspector, PrimNamePredicate(proceduralName));
}
