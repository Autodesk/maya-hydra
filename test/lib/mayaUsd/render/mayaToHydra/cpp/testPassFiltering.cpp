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
#include <pxr/imaging/hd/sceneIndex.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool isFilteredOut(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath)
{
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    return prim.primType.IsEmpty() && !prim.dataSource;
}

bool isPresent(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath, const TfToken& primType)
{
    HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
    return prim.primType == primType && prim.dataSource;
}

bool findSphereMeshPredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
    return primPath.GetString().find("StandardShadedItem") != std::string::npos;
};

bool findSphereWireframePredicate(const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) {
    return primPath.GetString().find("DormantPolyWire") != std::string::npos;
};

} // namespace

TEST(PassFiltering, testPassFiltering)
{
    std::vector<HdSceneIndexBasePtr> passSceneIndices = {
        GetPassSceneIndex(0),
        GetPassSceneIndex(1)
    };
    
    // Checks that a prim is present in the given passes, and filtered out in the others.
    auto testPrim = [&](const SdfPath& primPath, const TfToken& primType, std::set<int> passIndices) {
        for (size_t iPassIndex = 0; iPassIndex < passSceneIndices.size(); iPassIndex++) {
            if (passIndices.find(iPassIndex) != passIndices.end()) {
                ASSERT_TRUE(isPresent(passSceneIndices[iPassIndex], primPath, primType)) << primPath.GetName() << " of type " << primType << " was not found in pass " << iPassIndex;
            } else {
                ASSERT_TRUE(isFilteredOut(passSceneIndices[iPassIndex], primPath)) << primPath.GetName() << " was found in pass " << iPassIndex;
            }
        }
    };

    auto sphereMeshPrims = SceneIndexInspector(passSceneIndices[0]).FindPrims(findSphereMeshPredicate);
    ASSERT_EQ(sphereMeshPrims.size(), 1u);
    auto sphereMeshPath = sphereMeshPrims.front().primPath;

    auto sphereWireframePrims = SceneIndexInspector(passSceneIndices[1]).FindPrims(findSphereWireframePredicate);
    ASSERT_EQ(sphereWireframePrims.size(), 1u);
    auto sphereWireframePath = sphereWireframePrims.front().primPath;

    // Maya mesh prim
    testPrim(
        SdfPath("/MayaHydraViewportRenderer/materials/openPBRSurface1SG"), 
        HdPrimTypeTokens->material,
        {0}
    );
    testPrim(
        sphereMeshPath, 
        HdPrimTypeTokens->mesh,
        {0}
    );
    testPrim(
        sphereWireframePath, 
        HdPrimTypeTokens->basisCurves,
        {1}
    );

    // USD prim using a material without displacement
    testPrim(
        SdfPath("/MayaUsdProxyShape_PluginNode/stageShape1/mtl/open_pbr_surface1"), 
        HdPrimTypeTokens->material,
        {0} // Should not be in the secondary graphics pass
    );
    testPrim(
        SdfPath("/FlowViewportSelectionHighlights/MayaUsdProxyShape_PluginNode/stageShape1/pTorus1/Highlight_/MayaUsdProxyShape_PluginNode/stageShape1/pTorus1"), 
        HdPrimTypeTokens->mesh,
        {1}
    );

    // USD prim using a material with displacement
    testPrim(
        SdfPath("/MayaUsdProxyShape_PluginNode/GeomSubsetWireframeHighlightDisplacementTestSceneShape/mtl/UsdPreviewSurface1"), 
        HdPrimTypeTokens->material,
        {0,1} // Should be in both passes
    );
    testPrim(
        SdfPath("/MayaUsdProxyShape_PluginNode/GeomSubsetWireframeHighlightDisplacementTestSceneShape/mtl/UsdPreviewSurface2"), 
        HdPrimTypeTokens->material,
        {0,1} // Should be in both passes
    );
    testPrim(
        SdfPath("/FlowViewportSelectionHighlights/MayaUsdProxyShape_PluginNode/GeomSubsetWireframeHighlightDisplacementTestSceneShape/SphereInstancer/Highlight_Full/MayaUsdProxyShape_PluginNode/GeomSubsetWireframeHighlightDisplacementTestSceneShape/SphereInstancer"), 
        HdPrimTypeTokens->instancer,
        {1}
    );
}
