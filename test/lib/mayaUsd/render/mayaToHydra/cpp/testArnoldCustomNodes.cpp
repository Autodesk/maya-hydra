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
// C++ GTest suite for mtoaSIP (Hydra Arnold Scene Index Plugin).
//
// Tests must run with the Arnold renderer active so that mtoaSIP is inserted
// into the scene index chain.  The Python driver (testArnoldCustomNodes.py) sets the
// Arnold renderer and creates the required scene nodes.
//
// Custom node translation:
//    aiPhotometricLight -> sphereLight   (verifyPrimType, attrs)
//    aiStandIn          -> ArnoldProcedural  (verifyPrimType, dso -> arnold:filename)
//    aiVolume           -> ArnoldVolume      (verifyPrimType, filename -> arnold:filename)
//
// NOTE: aiSkyDomeLight and aiAreaLight are NOT tested via the mayaCustomDagNode
// path because they have existing maya-hydra adapters (aiSkydomeLightAdapter /
// aiAreaLightAdapter) that produce domeLight / rectLight prims directly, so
// mtoaSIP's custom node dispatch never sees them.
//

#include "testUtils.h"

#include <mayaHydraLib/adapters/tokens.h>

#include <pxr/base/gf/vec3f.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usd/sdf/assetPath.h>
#include <pxr/usd/usdLux/tokens.h>

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

// What: mtoaSIP re-types aiPhotometricLight to sphereLight and maps attributes.
// How:  Python creates aiPhotometricLight (intensity=2.5, aiExposure=3.0,
//       aiFilename="/path/to/test.ies", aiSamples=5) with Arnold renderer active.
// Expect: prim type = sphereLight; inputs:intensity, inputs:exposure,
//         inputs:shaping:ies:file are set; arnold:samples primvar exists (catch-all).
TEST(ArnoldCustomNodes, photometricLightTranslation)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr si = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->sphereLight);
    ASSERT_TRUE(si) << "sphereLight prim for '" << shapeNamePart
                    << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->sphereLight), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_EQ(prim.primType, HdPrimTypeTokens->sphereLight);
    ASSERT_NE(prim.dataSource, nullptr);

    // Check inputs:intensity = 2.5
    auto intensityDs
        = HdSampledDataSource::Cast(prim.dataSource->Get(UsdLuxTokens->inputsIntensity));
    ASSERT_TRUE(intensityDs) << "inputs:intensity not found on sphereLight";
    EXPECT_NEAR(GetFloatValue(intensityDs), 2.5f, 1e-5f);

    // aiExposure = 3.0 -> inputs:exposure
    auto exposureDs = HdSampledDataSource::Cast(prim.dataSource->Get(UsdLuxTokens->inputsExposure));
    ASSERT_TRUE(exposureDs) << "inputs:exposure not found on sphereLight";
    EXPECT_NEAR(GetFloatValue(exposureDs), 3.0f, 1e-5f);

    // aiFilename -> inputs:shaping:ies:file
    auto iesDs = HdTypedSampledDataSource<SdfAssetPath>::Cast(
        prim.dataSource->Get(UsdLuxTokens->inputsShapingIesFile));
    ASSERT_TRUE(iesDs) << "inputs:shaping:ies:file not found on sphereLight";
    EXPECT_EQ(iesDs->GetTypedValue(0.0f).GetAssetPath(), "/path/to/test.ies");

    // aiSamples = 5 -> arnold:samples primvar (catch-all ai* pass-through).
    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);
    HdPrimvarSchema  samplesSchema = primvarsSchema.GetPrimvar(TfToken("arnold:samples"));
    EXPECT_TRUE(samplesSchema)
        << "arnold:samples primvar not found; aiSamples not forwarded by mtoaSIP";
}

// What: mtoaSIP re-types aiStandIn to ArnoldProcedural and maps dso -> arnold:filename.
// How:  Python creates aiStandIn (dso="/path/to/test.ass") with Arnold renderer.
// Expect: prim type = ArnoldProcedural; arnold:filename = "/path/to/test.ass".
TEST(ArnoldCustomNodes, standInTranslation)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    static const TfToken kArnoldProcedural("ArnoldProcedural");
    static const TfToken kArnoldFilename("arnold:filename");

    HdSceneIndexBaseRefPtr si
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, kArnoldProcedural);
    ASSERT_TRUE(si) << "ArnoldProcedural prim for '" << shapeNamePart
                    << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, kArnoldProcedural), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_EQ(prim.primType, kArnoldProcedural);
    ASSERT_NE(prim.dataSource, nullptr);

    // dso -> arnold:filename
    auto filenameDs
        = HdTypedSampledDataSource<std::string>::Cast(prim.dataSource->Get(kArnoldFilename));
    ASSERT_TRUE(filenameDs) << "arnold:filename not found on ArnoldProcedural";
    EXPECT_EQ(filenameDs->GetTypedValue(0.0f), "/path/to/test.ass");
}

// What: mtoaSIP re-types aiVolume to ArnoldVolume and maps filename -> arnold:filename.
// How:  Python creates aiVolume (filename="/path/to/test.vdb") with Arnold renderer.
// Expect: prim type = ArnoldVolume; arnold:filename = "/path/to/test.vdb".
TEST(ArnoldCustomNodes, volumeTranslation)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    static const TfToken kArnoldVolume("ArnoldVolume");
    static const TfToken kArnoldFilename("arnold:filename");

    HdSceneIndexBaseRefPtr si
        = FindTerminalSceneIndexWithPrim(sceneIndices, shapeNamePart, kArnoldVolume);
    ASSERT_TRUE(si) << "ArnoldVolume prim for '" << shapeNamePart
                    << "' not found in any scene index";

    SceneIndexInspector inspector(si);
    PrimEntriesVector   found
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, kArnoldVolume), 1);
    ASSERT_GE(found.size(), 1u);

    const HdSceneIndexPrim& prim = found.front().prim;
    ASSERT_EQ(prim.primType, kArnoldVolume);
    ASSERT_NE(prim.dataSource, nullptr);

    // filename (aiVolume Maya attr) -> arnold:filename
    // _TranslateVolume prefixes ALL mayaAttributes keys with "arnold:".
    // The value is stored via _VtValueDataSource, so use the base HdSampledDataSource.
    auto filenameBase = prim.dataSource->Get(kArnoldFilename);
    ASSERT_TRUE(filenameBase) << "arnold:filename not found on ArnoldVolume — "
                                 "all-attrs-as-arnold:* pattern not applied by mtoaSIP";

    // The value is stored via _VtValueDataSource, so use the base HdSampledDataSource.
    auto filenameDs = HdSampledDataSource::Cast(filenameBase);
    ASSERT_TRUE(filenameDs) << "arnold:filename is not a sampled data source";
    const VtValue filenameVal = filenameDs->GetValue(0.0f);
    ASSERT_TRUE(filenameVal.IsHolding<std::string>()) << "arnold:filename value is not a string";
    EXPECT_EQ(filenameVal.UncheckedGet<std::string>(), "/path/to/test.vdb");
}
