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

#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kLightShapeOptionVar = "mhLightShape";
const char* kLightShapeFallback = "aiSkyDomeLightShape1";

// Search for a primvars-dirty entry whose primvar value matches an expected float.
bool FindDirtyPrimWithPrimvarValueSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const TfToken&                      primvarName,
    float                               expectedValue)
{
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    auto        sceneIndex = accumulator.GetObservedSceneIndex();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (!entries[i].dirtyLocators.Contains(HdPrimvarsSchema::GetDefaultLocator())) {
            continue;
        }

        HdSceneIndexPrim prim = sceneIndex->GetPrim(entries[i].primPath);
        if (!prim.dataSource) {
            continue;
        }

        HdSampledDataSourceHandle valueDs
            = HdPrimvarsSchema::GetFromParent(prim.dataSource)
                  .GetPrimvar(primvarName)
                  .GetPrimvarValue();
        if (!valueDs) {
            continue;
        }

        VtValue value = valueDs->GetValue(0.0f);
        float   actual = 0.0f;
        if (value.IsHolding<float>()) {
            actual = value.UncheckedGet<float>();
        } else if (value.IsHolding<double>()) {
            actual = static_cast<float>(value.UncheckedGet<double>());
        } else {
            continue;
        }
        if (std::abs(actual - expectedValue) <= 1e-5f) {
            return true;
        }
    }
    return false;
}

// Scan dirtied entries for a light prim and report primvars vs light-schema dirty.
void CheckLightDirtySince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      lightPrimPath,
    bool&                               outHadPrimvarsDirty,
    bool&                               outHadLightSchemaDirty)
{
    outHadPrimvarsDirty = false;
    outHadLightSchemaDirty = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto  lightSchemaLocator = HdLightSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != lightPrimPath) {
            continue;
        }
        if (entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            outHadPrimvarsDirty = true;
        }
        if (entries[i].dirtyLocators.Intersects(lightSchemaLocator)) {
            outHadLightSchemaDirty = true;
        }
    }
}

// Count primvars-dirty notices for the light prim since a starting index.
size_t CountPrimvarsDirtyEntriesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const SdfPath&                      lightPrimPath)
{
    size_t count = 0;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    const auto  primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath == lightPrimPath
            && entries[i].dirtyLocators.Intersects(primvarsLocator)) {
            ++count;
        }
    }
    return count;
}

} // namespace

// What: non-default extension attrs appear as primvars; defaults remain absent.
// How: validate initial primvars, then update aiExposure and observe dirtied primvar values.
// Expect: aiExposure/aiDiffuse primvars exist at non-default values; default aiSpecular absent.
TEST(LightPrimvars, testTranslationAndDirtying)
{
    const std::string lightShapeFull = GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(lightShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    // ----- Test case 1: Translation (non-default present, default absent) -----
    // Find which scene index contains the light - we must observe that same index for dirty
    // notifications, since different terminal indices may not all receive Maya light data.
    HdSceneIndexBaseRefPtr sceneIndexWithLight = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->domeLight);
    ASSERT_TRUE(sceneIndexWithLight) << "aiSkyDomeLight prim not found in scene index";

    SceneIndexInspector inspector(sceneIndexWithLight);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->domeLight), 1);
    ASSERT_GE(foundPrims.size(), 1u);

    HdSceneIndexPrim prim = foundPrims.front().prim;
    ASSERT_EQ(prim.primType, HdPrimTypeTokens->domeLight);
    ASSERT_NE(prim.dataSource, nullptr);

    HdPrimvarsSchema primvarsSchema = HdPrimvarsSchema::GetFromParent(prim.dataSource);

    // Non-default attrs present: aiExposure=2.0, aiDiffuse=0.5
    HdPrimvarSchema aiExposureSchema = primvarsSchema.GetPrimvar(TfToken("aiExposure"));
    ASSERT_TRUE(aiExposureSchema.GetPrimvarValue())
        << "aiExposure (non-default) should appear as primvar";
    VtValue aiExposureVal = aiExposureSchema.GetPrimvarValue()->GetValue(0.0f);
    ASSERT_TRUE(aiExposureVal.IsHolding<float>() || aiExposureVal.IsHolding<double>())
        << "aiExposure should be float or double";
    const float aiExposureFloat = aiExposureVal.IsHolding<float>()
        ? aiExposureVal.UncheckedGet<float>()
        : static_cast<float>(aiExposureVal.UncheckedGet<double>());
    EXPECT_FLOAT_EQ(aiExposureFloat, 2.0f);

    HdPrimvarSchema aiDiffuseSchema = primvarsSchema.GetPrimvar(TfToken("aiDiffuse"));
    ASSERT_TRUE(aiDiffuseSchema.GetPrimvarValue())
        << "aiDiffuse (non-default) should appear as primvar";
    VtValue aiDiffuseVal = aiDiffuseSchema.GetPrimvarValue()->GetValue(0.0f);
    ASSERT_TRUE(aiDiffuseVal.IsHolding<float>() || aiDiffuseVal.IsHolding<double>())
        << "aiDiffuse should be float or double";
    const float aiDiffuseFloat = aiDiffuseVal.IsHolding<float>()
        ? aiDiffuseVal.UncheckedGet<float>()
        : static_cast<float>(aiDiffuseVal.UncheckedGet<double>());
    EXPECT_FLOAT_EQ(aiDiffuseFloat, 0.5f);

    // Default-value attr absent: aiSpecular at 1.0
    HdPrimvarSchema aiSpecularSchema = primvarsSchema.GetPrimvar(TfToken("aiSpecular"));
    EXPECT_FALSE(aiSpecularSchema.GetPrimvarValue())
        << "aiSpecular (default) should NOT appear as primvar";

    // ----- Test case 2: Dirty notification and value update -----
    // Observe MayaHydraSceneIndex - it's the one that sends dirty when Maya attrs change.
    // Terminal scene indices may not propagate these notifications.
    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithLight);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    MGlobal::executeCommand(
        ("setAttr \"" + lightShapeFull + ".aiExposure\" 3.5").c_str());
    MGlobal::executeCommand("refresh");

    EXPECT_TRUE(FindDirtyPrimWithPrimvarValueSince(
        notifsAccumulator,
        startIndex,
        TfToken("aiExposure"),
        3.5f))
        << "Dirty notification with updated aiExposure=3.5 should have been received";
}

// What: non-param light attributes should dirty primvars only.
// How: set aiShadowDensity (primvar-only) and inspect dirtied locators.
// Expect: primvars dirty is present; light schema dirty is absent.
TEST(LightPrimvars, NonParamAttrTriggersOnlyPrimvarDirty)
{
    const std::string lightShapeFull = GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(lightShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    // Find which scene index contains the light - observe that same index for dirty notifications
    HdSceneIndexBaseRefPtr sceneIndexWithLight = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->domeLight);
    ASSERT_TRUE(sceneIndexWithLight) << "aiSkyDomeLight prim not found in any scene index";

    // Observe MayaHydraSceneIndex - it sends dirty when Maya attrs change.
    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithLight);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    // Get light path from MayaHydraSceneIndex (paths may differ from terminal)
    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->domeLight), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "aiSkyDomeLight not found in MayaHydraSceneIndex";
    const SdfPath lightPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change aiShadowDensity (not in param attr list - primvar-only); setup sets it to 0
    MGlobal::executeCommand(
        ("setAttr \"" + lightShapeFull + ".aiShadowDensity\" 0.5").c_str());
    MGlobal::executeCommand("refresh");

    bool hadPrimvarsDirty = false;
    bool hadLightSchemaDirty = false;
    CheckLightDirtySince(
        notifsAccumulator, startIndex, lightPrimPath, hadPrimvarsDirty, hadLightSchemaDirty);

    EXPECT_TRUE(hadPrimvarsDirty)
        << "Changing aiShadowDensity (non-param-attr) should trigger DirtyPrimvar";
    EXPECT_FALSE(hadLightSchemaDirty)
        << "Changing aiShadowDensity (non-param-attr) must NOT trigger DirtyParams - "
           "only primvars dirty expected";
}

// What: param attribute updates should not duplicate primvars dirty notices.
// How: change aiExposure and count primvars dirty entries since the change.
// Expect: exactly one primvars dirty entry for the light.
TEST(LightPrimvars, IntensityUpdateNoDuplicatePrimvarsDirty)
{
    const std::string lightShapeFull = GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeFallback);
    const std::string shapeNamePart = GetShapeNameFromFullPath(lightShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr sceneIndexWithLight = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, HdPrimTypeTokens->domeLight);
    ASSERT_TRUE(sceneIndexWithLight) << "aiSkyDomeLight prim not found in any scene index";

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndexWithLight);
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector inspector(mayaSceneIndex);
    PrimEntriesVector   foundPrims
        = inspector.FindPrims(CreatePrimPredicate(shapeNamePart, HdPrimTypeTokens->domeLight), 1);
    ASSERT_GE(foundPrims.size(), 1u) << "aiSkyDomeLight not found in MayaHydraSceneIndex";
    const SdfPath lightPrimPath = foundPrims.front().primPath;

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    // Change aiExposure (intensity for Arnold lights) - param attribute
    MGlobal::executeCommand(
        ("setAttr \"" + lightShapeFull + ".aiExposure\" 4.0").c_str());
    MGlobal::executeCommand("refresh");

    const size_t primvarsDirtyCount = CountPrimvarsDirtyEntriesSince(
        notifsAccumulator, startIndex, lightPrimPath);

    EXPECT_EQ(primvarsDirtyCount, 1u)
        << "Updating aiExposure (intensity) should trigger exactly one primvars dirty "
           "notification, got " << primvarsDirtyCount << " (duplicate notifications)";
}

// What: light param attributes list must match the adapter's attribute usage.
// How: compare an expected list against the adapter's param attribute set.
// Expect: all attributes used by the light adapter logic are present in the set.
TEST(LightPrimvars, ParamAttributesMatchGetLogic)
{
    static const std::vector<std::string> kAttrsUsedInGetLogic = {
        // GetMayaLightParams
        "intensity", "color", "shadowColor",
        "aiExposure", "aiNormalize", "aiDiffuse", "aiSpecular",
        "aiEnableTemperature", "aiColorTemperature",
        // _CalculateShadowParams, _CalculateShadowProjectionMatrix
        "decayRate", "emitDiffuse", "emitSpecular",
        "dmapResolution", "dmapBias", "dmapFilterSize",
        "useDmapAutoFocus", "dmapWidthFocus", "dmapFarClipPlane", "dmapNearClipPlane",
        "coneAngle", "dropoff", "lightAngle",
        // GetLightMaterialNetwork (dome lights)
        "format",
        // Shadow-related Arnold attrs (affect GetShadowsEnabled / light behavior)
        "aiCastVolumetricShadows", "aiVolumeSamples", "aiCastShadows",
    };

    const auto& paramAttrs = MayaHydraLightAdapter::GetLightParamAttributeNamesForTest();
    for (const std::string& attr : kAttrsUsedInGetLogic) {
        EXPECT_TRUE(paramAttrs.count(attr)) << "Attribute '" << attr
            << "' is used in GetMayaLightParams/GetLightParamValue/_CalculateShadowParams/"
               "_CalculateShadowProjectionMatrix/GetLightMaterialNetwork but is missing from "
               "kLightParamAttributeNames in lightAdapter.cpp. Add it to keep the param attr list in sync.";
    }
}
