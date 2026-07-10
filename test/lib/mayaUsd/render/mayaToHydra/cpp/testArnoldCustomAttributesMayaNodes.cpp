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
// C++ GTest suite for Arnold custom attribute translation on native Maya nodes.
//
// Tests must run with the Arnold renderer active so that mtoaSIP is inserted
// into the scene index chain.  The Python driver (testArnoldCustomAttributesMayaNodes.py) sets the
// Arnold renderer and creates the required scene nodes.
//
// Mesh/Camera ai* attrs -> arnold:* primvars (mtoaSIP _RemapMtoaAiName).
// Light param attrs (aiExposure, aiColorTemperature) -> HdLight schema (light.exposure,
// light.colorTemperature) via MayaHydraLightAdapter.
// Light non-param ai* attrs (aiAngle, aiResolution, aiSpread, aiRoundness) ->
// arnold:* primvars.
//

#include "testUtils.h"

#include <mayaHydraLib/mayaUtils.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/light.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>

#include <gtest/gtest.h>

#include <string>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

namespace {

// Convenience: read a float from a sampled data source, accepting both float
// and double storage (MtoA stores many attrs as double).
float GetFloatValue(const HdSampledDataSourceHandle& ds, float fallback = 0.0f)
{
    if (!ds)
        return fallback;
    const VtValue v = ds->GetValue(0.0f);
    if (v.IsHolding<float>())
        return v.UncheckedGet<float>();
    if (v.IsHolding<double>())
        return static_cast<float>(v.UncheckedGet<double>());
    return fallback;
}

} // namespace

// What: mtoaSIP remaps mesh ai* attrs to arnold:* primvars.
// How:  Python creates mesh with non-default aiSubdivType, aiDispHeight, aiOpaque,
//       aiMatte, aiSelfShadows.
// Expect: arnold:subdiv_type, disp_height, opaque, matte, self_shadows primvars match.
TEST(ArnoldCustomAttributesMayaNodes, meshArnoldPrimvars)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr si
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, HdPrimTypeTokens->mesh);
    ASSERT_TRUE(si) << "mesh prim for '" << shapeNamePart << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->mesh), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    EXPECT_EQ(
        primvarsSchema.GetPrimvar(TfToken("arnold:subdiv_type"))
            .GetPrimvarValue()
            ->GetValue(0.0f)
            .UncheckedGet<int>(),
        1)
        << "aiSubdivType should map to arnold:subdiv_type";
    EXPECT_NEAR(
        GetFloatValue(primvarsSchema.GetPrimvar(TfToken("arnold:disp_height")).GetPrimvarValue()),
        0.25f,
        1e-5f)
        << "aiDispHeight should map to arnold:disp_height";
    EXPECT_FALSE(primvarsSchema.GetPrimvar(TfToken("arnold:opaque"))
                     .GetPrimvarValue()
                     ->GetValue(0.0f)
                     .UncheckedGet<bool>())
        << "aiOpaque=0 should map to arnold:opaque";
    EXPECT_TRUE(primvarsSchema.GetPrimvar(TfToken("arnold:matte"))
                    .GetPrimvarValue()
                    ->GetValue(0.0f)
                    .UncheckedGet<bool>())
        << "aiMatte=1 should map to arnold:matte";
    EXPECT_FALSE(primvarsSchema.GetPrimvar(TfToken("arnold:self_shadows"))
                     .GetPrimvarValue()
                     ->GetValue(0.0f)
                     .UncheckedGet<bool>())
        << "aiSelfShadows=0 should map to arnold:self_shadows";
}

// What: mtoaSIP remaps camera ai* attrs to arnold:* primvars.
// How:  Python creates camera with aiExposure=1.0, aiFocusDistance=12.5.
// Expect: arnold:exposure and arnold:focus_distance primvars match.
TEST(ArnoldCustomAttributesMayaNodes, cameraArnoldPrimvars)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr si
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, HdPrimTypeTokens->camera);
    ASSERT_TRUE(si) << "camera prim for '" << shapeNamePart << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->camera), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    EXPECT_NEAR(
        GetFloatValue(primvarsSchema.GetPrimvar(TfToken("arnold:exposure")).GetPrimvarValue()),
        1.0f,
        1e-5f)
        << "aiExposure should map to arnold:exposure";
    EXPECT_NEAR(
        GetFloatValue(
            primvarsSchema.GetPrimvar(TfToken("arnold:focus_distance")).GetPrimvarValue()),
        12.5f,
        1e-5f)
        << "aiFocusDistance should map to arnold:focus_distance";
}

// What: directionalLight ai* attrs map to light schema inputs and arnold:* primvars.
// How:  Python creates directionalLight with aiAngle=2.5, aiExposure=1.5,
//       aiCastVolumetricShadows=0, aiColorTemperature=3200 (temperature enabled).
// Expect: arnold:angle and arnold:cast_volumetric_shadows primvars;
//         light.exposure and light.colorTemperature on distantLight.
TEST(ArnoldCustomAttributesMayaNodes, directionalLightAttributes)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr si = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->distantLight);
    ASSERT_TRUE(si) << "distantLight prim for '" << shapeNamePart
                    << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->distantLight), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);
    ASSERT_EQ(prim.primType, HdPrimTypeTokens->distantLight);

    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    EXPECT_NEAR(
        GetFloatValue(primvarsSchema.GetPrimvar(TfToken("arnold:angle")).GetPrimvarValue()),
        2.5f,
        1e-5f)
        << "aiAngle should map to arnold:angle";

    MObject lightNode;
    ASSERT_TRUE(GetDependNodeFromNodeName(shapeNamePart.c_str(), lightNode));
    MFnDependencyNode lightDepNode(lightNode);
    const MPlug       castVolShadowsPlug = lightDepNode.findPlug("aiCastVolumetricShadows", true);
    ASSERT_FALSE(castVolShadowsPlug.isNull());
    const bool expectedCastVolShadows = castVolShadowsPlug.asBool();
    EXPECT_EQ(
        primvarsSchema.GetPrimvar(TfToken("arnold:cast_volumetric_shadows"))
            .GetPrimvarValue()
            ->GetValue(0.0f)
            .UncheckedGet<bool>(),
        expectedCastVolShadows)
        << "aiCastVolumetricShadows should map to arnold:cast_volumetric_shadows";

    HdLightSchema lightSchema = HdLightSchema::GetFromParent(prim.dataSource);
    ASSERT_TRUE(lightSchema);
    auto container = lightSchema.GetContainer();
    ASSERT_TRUE(container);
    auto exposureDs = HdSampledDataSource::Cast(container->Get(HdLightTokens->exposure));
    EXPECT_NEAR(GetFloatValue(exposureDs), 1.5f, 1e-5f)
        << "aiExposure should map to light.exposure";
    auto colorTempDs = HdSampledDataSource::Cast(container->Get(HdLightTokens->colorTemperature));
    EXPECT_NEAR(GetFloatValue(colorTempDs), 3200.0f, 1e-3f)
        << "aiColorTemperature should map to light.colorTemperature";
}

// What: mtoaSIP remaps areaLight ai* attrs to arnold:* primvars.
// How:  Python creates areaLight with aiResolution=256, aiSpread=0.5, aiRoundness=0.25.
// Expect: arnold:resolution, arnold:spread, arnold:roundness primvars match.
TEST(ArnoldCustomAttributesMayaNodes, areaLightArnoldPrimvars)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr si
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, HdPrimTypeTokens->rectLight);
    ASSERT_TRUE(si) << "rectLight prim for '" << shapeNamePart << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->rectLight), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);
    ASSERT_EQ(prim.primType, HdPrimTypeTokens->rectLight);

    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    EXPECT_EQ(
        primvarsSchema.GetPrimvar(TfToken("arnold:resolution"))
            .GetPrimvarValue()
            ->GetValue(0.0f)
            .UncheckedGet<int>(),
        256)
        << "aiResolution should map to arnold:resolution";
    EXPECT_NEAR(
        GetFloatValue(primvarsSchema.GetPrimvar(TfToken("arnold:spread")).GetPrimvarValue()),
        0.5f,
        1e-5f)
        << "aiSpread should map to arnold:spread";
    EXPECT_NEAR(
        GetFloatValue(primvarsSchema.GetPrimvar(TfToken("arnold:roundness")).GetPrimvarValue()),
        0.25f,
        1e-5f)
        << "aiRoundness should map to arnold:roundness";
}
