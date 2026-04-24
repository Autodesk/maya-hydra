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

#include <pxr/imaging/hd/extComputationPrimvarsSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/collectionsSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MGlobal.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>
#include <maya/MStatus.h>

#include <gtest/gtest.h>

#include <cmath>
#include <sstream>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

const char* kAttrName = "extDirty";
const char* kMeshShapeName = "dirtyMeshShape";
const char* kCameraShapeName = "dirtyCameraShape";
const char* kLightShapeName = "dirtyLightShape";
const char* kMaterialSgName = "dirtyMaterialSG";
const char* kMeshShapeOptionVar = "mhDirtyMeshShape";
const char* kCameraShapeOptionVar = "mhDirtyCameraShape";
const char* kLightShapeOptionVar = "mhDirtyLightShape";
const char* kMaterialSgOptionVar = "mhDirtyMaterialSg";

// Guard tests when Arnold attributes are missing (e.g., plugin not loaded).
bool PlugExists(const std::string& nodeName, const char* plugName)
{
    MSelectionList selection;
    if (selection.add(nodeName.c_str()) != MStatus::kSuccess) {
        return false;
    }

    MObject nodeObj;
    if (selection.getDependNode(0, nodeObj) != MStatus::kSuccess || nodeObj.isNull()) {
        return false;
    }

    MStatus status;
    MFnDependencyNode depNode(nodeObj);
    depNode.findPlug(plugName, true, &status);
    return status == MStatus::kSuccess;
}

// Traverse terminal indices to find the MayaHydraSceneIndex that owns the prim.
HdSceneIndexBaseRefPtr FindMayaSceneIndexForShape(
    const SceneIndicesVector& sceneIndices,
    const std::string&        shapeNamePart)
{
    HdSceneIndexBaseRefPtr sceneIndexWithShape = FindTerminalSceneIndexWithPrim(
        sceneIndices, PrimNamePredicate(shapeNamePart));
    if (!sceneIndexWithShape) {
        return nullptr;
    }
    return FindMayaHydraSceneIndex(sceneIndexWithShape);
}

// Use setAttr to author a Maya attribute, then refresh to flush notifications.
bool SetAttrAndRefreshValue(
    const std::string& nodeName,
    const std::string& attrName,
    const std::string& valueLiteral)
{
    const std::string cmd = std::string("setAttr \"") + nodeName + "." + attrName + "\" "
        + valueLiteral;
    const bool success = (MGlobal::executeCommand(cmd.c_str()) == MStatus::kSuccess);
    MGlobal::executeCommand("refresh");
    return success;
}

// Convenience wrapper for the shared test attribute used in testDirtyPrimvars.
void SetAttrAndRefresh(const std::string& nodeName, double value)
{
    SetAttrAndRefreshValue(nodeName, kAttrName, std::to_string(value));
}

struct DirtyNoticeSummary
{
    size_t totalEntries = 0;
    size_t matchingEntries = 0;
    size_t primvarsCount = 0;
    size_t extCompPrimvarsCount = 0;
    size_t combinedPrimvarsExtCompCount = 0;
    bool   hasUnexpectedPrim = false;
    bool   hasUnexpectedLocator = false;
};

struct LightParamNoticeSummary
{
    size_t totalEntries = 0;
    size_t matchingEntries = 0;
    size_t lightSchemaCount = 0;
    size_t primvarsCount = 0;
    size_t visibilityCount = 0;
    size_t collectionsCount = 0;
    size_t extCompPrimvarsCount = 0;
    bool   hasUnexpectedPrim = false;
    bool   hasUnexpectedLocator = false;
};

// Walk dirtied entries and summarize counts/locators for the prim(s) of interest.
DirtyNoticeSummary SummarizeDirtyNoticesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const std::string&                  expectedPrimNamePart,
    const std::string&                  alternatePrimNamePart,
    bool                                allowExtCompPrimvars)
{
    DirtyNoticeSummary summary;
    const auto&        entries = accumulator.GetDirtiedPrimEntries();
    const auto         primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto         extCompLocator = HdExtComputationPrimvarsSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        ++summary.totalEntries;
        const std::string primPathStr = entries[i].primPath.GetAsString();
        const bool matchesPrim = (expectedPrimNamePart.empty() && alternatePrimNamePart.empty())
            || (primPathStr.find(expectedPrimNamePart) != std::string::npos)
            || (!alternatePrimNamePart.empty()
                && primPathStr.find(alternatePrimNamePart) != std::string::npos);
        if (!matchesPrim) {
            summary.hasUnexpectedPrim = true;
        }

        const HdDataSourceLocatorSet& locators = entries[i].dirtyLocators;
        if (locators.IsEmpty()) {
            summary.hasUnexpectedLocator = true;
            continue;
        }

        const bool hasPrimvars = locators.Intersects(primvarsLocator);
        const bool hasExtComp = allowExtCompPrimvars && locators.Intersects(extCompLocator);
        if (matchesPrim) {
            ++summary.matchingEntries;
            if (hasPrimvars) {
                ++summary.primvarsCount;
            }
            if (hasExtComp) {
                ++summary.extCompPrimvarsCount;
            }
            if (hasPrimvars && hasExtComp) {
                ++summary.combinedPrimvarsExtCompCount;
            }
        }

        for (auto it = locators.begin(); it != locators.end(); ++it) {
            const HdDataSourceLocator& locator = *it;
            if (locator.HasPrefix(primvarsLocator)) {
                continue;
            }
            if (allowExtCompPrimvars && locator.HasPrefix(extCompLocator)) {
                continue;
            }
            summary.hasUnexpectedLocator = true;
        }
    }

    return summary;
}

// Summarize dirtied entries for light params, allowing light/primvars/visibility/collections.
LightParamNoticeSummary SummarizeLightParamNoticesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const std::string&                  expectedPrimNamePart,
    const std::string&                  alternatePrimNamePart)
{
    LightParamNoticeSummary summary;
    const auto&             entries = accumulator.GetDirtiedPrimEntries();
    const auto              lightSchemaLocator = HdLightSchema::GetDefaultLocator();
    const auto              primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();
    const auto              visibilityLocator = HdVisibilitySchema::GetDefaultLocator();
    const auto              collectionsLocator = HdCollectionsSchema::GetDefaultLocator();
    const auto              extCompLocator = HdExtComputationPrimvarsSchema::GetDefaultLocator();

    for (size_t i = startIndex; i < entries.size(); ++i) {
        ++summary.totalEntries;
        const std::string primPathStr = entries[i].primPath.GetAsString();
        const bool matchesPrim = (expectedPrimNamePart.empty() && alternatePrimNamePart.empty())
            || (primPathStr.find(expectedPrimNamePart) != std::string::npos)
            || (!alternatePrimNamePart.empty()
                && primPathStr.find(alternatePrimNamePart) != std::string::npos);
        if (!matchesPrim) {
            summary.hasUnexpectedPrim = true;
        }

        const HdDataSourceLocatorSet& locators = entries[i].dirtyLocators;
        if (locators.IsEmpty()) {
            summary.hasUnexpectedLocator = true;
            continue;
        }

        const bool hasLightSchema = locators.Intersects(lightSchemaLocator);
        const bool hasPrimvars = locators.Intersects(primvarsLocator);
        const bool hasVisibility = locators.Intersects(visibilityLocator);
        const bool hasCollections = locators.Intersects(collectionsLocator);
        const bool hasExtComp = locators.Intersects(extCompLocator);
        if (matchesPrim) {
            ++summary.matchingEntries;
            if (hasLightSchema) {
                ++summary.lightSchemaCount;
            }
            if (hasPrimvars) {
                ++summary.primvarsCount;
            }
            if (hasVisibility) {
                ++summary.visibilityCount;
            }
            if (hasCollections) {
                ++summary.collectionsCount;
            }
            if (hasExtComp) {
                ++summary.extCompPrimvarsCount;
            }
        }

        for (auto it = locators.begin(); it != locators.end(); ++it) {
            const HdDataSourceLocator& locator = *it;
            if (locator.HasPrefix(lightSchemaLocator)) {
                continue;
            }
            if (locator.HasPrefix(primvarsLocator)) {
                continue;
            }
            if (locator.HasPrefix(visibilityLocator)) {
                continue;
            }
            if (locator.HasPrefix(collectionsLocator)) {
                continue;
            }
            summary.hasUnexpectedLocator = true;
        }
    }

    return summary;
}

// Human-readable dump to debug unexpected dirty entries or locators.
std::string DescribeDirtyEntriesSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex)
{
    std::ostringstream out;
    const auto&        entries = accumulator.GetDirtiedPrimEntries();
    out << "Dirty entries since index " << startIndex << ":\n";
    for (size_t i = startIndex; i < entries.size(); ++i) {
        out << "  [" << i << "] " << entries[i].primPath.GetAsString() << " locators: ";
        const HdDataSourceLocatorSet& locators = entries[i].dirtyLocators;
        if (locators.IsEmpty()) {
            out << "<empty>";
        } else {
            bool first = true;
            for (auto it = locators.begin(); it != locators.end(); ++it) {
                if (!first) {
                    out << ", ";
                }
                first = false;
                out << it->GetString();
            }
        }
        out << "\n";
    }
    return out.str();
}

// Validate a light param update: exactly one notice, light schema plus USD extra locators,
// and no extComputationPrimvars.
void ExpectLightParamNotice(
    const LightParamNoticeSummary&      summary,
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const char*                         valueLabel)
{
    EXPECT_EQ(summary.totalEntries, 1u)
        << "Expected exactly one dirtied notice for " << valueLabel << ", got "
        << summary.totalEntries;
    EXPECT_FALSE(summary.hasUnexpectedPrim)
        << "Dirtied notice did not match expected " << valueLabel << " prim name";
    EXPECT_FALSE(summary.hasUnexpectedLocator)
        << "Light parameter updates should dirty only light/primvars/visibility/collections locators";
    EXPECT_EQ(summary.matchingEntries, 1u)
        << "Expected exactly one notice for " << valueLabel << ", got " << summary.matchingEntries;
    EXPECT_EQ(summary.lightSchemaCount, 1u)
        << "Expected light-schema dirty for " << valueLabel << ", got "
        << summary.lightSchemaCount;
    EXPECT_EQ(summary.primvarsCount, 1u)
        << "Expected primvars dirty for " << valueLabel << ", got " << summary.primvarsCount;
    EXPECT_EQ(summary.visibilityCount, 1u)
        << "Expected visibility dirty for " << valueLabel << ", got " << summary.visibilityCount;
    EXPECT_EQ(summary.collectionsCount, 1u)
        << "Expected collections dirty for " << valueLabel << ", got " << summary.collectionsCount;
    EXPECT_EQ(summary.extCompPrimvarsCount, 0u)
        << "Expected no extComputationPrimvars dirty for " << valueLabel << ", got "
        << summary.extCompPrimvarsCount;

    if (summary.totalEntries != 1u || summary.hasUnexpectedLocator || summary.hasUnexpectedPrim) {
        ADD_FAILURE() << DescribeDirtyEntriesSince(accumulator, startIndex);
    }
}

// Shared validation: expect one matching notice, no unexpected prims/locators.
void ExpectSingleNoticeCommon(
    const DirtyNoticeSummary&           summary,
    SceneIndexNotificationsAccumulator& accumulator,
    size_t                              startIndex,
    const char*                         valueLabel,
    const char*                         locatorMessage)
{
    EXPECT_EQ(summary.totalEntries, 1u)
        << "Expected exactly one dirtied notice for " << valueLabel << ", got "
        << summary.totalEntries;
    EXPECT_FALSE(summary.hasUnexpectedPrim)
        << "Dirtied notice did not match expected " << valueLabel << " prim name";
    EXPECT_FALSE(summary.hasUnexpectedLocator) << locatorMessage;
    EXPECT_EQ(summary.matchingEntries, 1u)
        << "Expected exactly one notice for " << valueLabel << ", got " << summary.matchingEntries;

    if (summary.totalEntries != 1u || summary.hasUnexpectedLocator || summary.hasUnexpectedPrim) {
        ADD_FAILURE() << DescribeDirtyEntriesSince(accumulator, startIndex);
    }
}

// Search for a primvar dirty entry whose value equals the expected test value.
bool FindDirtyPrimWithPrimvarValueSince(
    SceneIndexNotificationsAccumulator& accumulator,
    size_t startIndex,
    double expectedValue)
{
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    auto sceneIndex = accumulator.GetObservedSceneIndex();
    const TfToken primvarName(kAttrName);

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
        if (!value.IsHolding<double>()) {
            continue;
        }

        const double actual = value.UncheckedGet<double>();
        if (std::abs(actual - expectedValue) <= 1e-6) {
            return true;
        }
    }
    return false;
}

} // namespace

// What: extension attribute changes should always dirty primvars.
// How: set a shared extension attribute on mesh/camera/light/material and refresh.
// Expect: at least one primvar-dirty entry contains the updated numeric value
//         for each node type.
TEST(MayaNodesAttributesDirtying, testDirtyPrimvars)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto checkDirty = [&](const std::string& nodeName, double value) {
        const std::string shapeNamePart = GetShapeNameFromFullPath(nodeName);
        auto              mayaSceneIndex = FindMayaSceneIndexForShape(sceneIndices, shapeNamePart);
        if (!mayaSceneIndex) {
            ADD_FAILURE() << "Prim not found in MayaHydraSceneIndex for node " << nodeName;
            return;
        }
        SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
        const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
        SetAttrAndRefresh(nodeName, value);
        EXPECT_TRUE(FindDirtyPrimWithPrimvarValueSince(notifsAccumulator, startIndex, value));
    };

    checkDirty(GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeName), 1.0);
    checkDirty(GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeName), 2.0);
    checkDirty(GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeName), 3.0);
    checkDirty(GetOptionVarOrDefault(kMaterialSgOptionVar, kMaterialSgName), 4.0);
}

// What: camera aiUScale (Arnold extension) should dirty only primvars.
// How: set aiUScale on the camera shape, refresh, then summarize dirtied entries.
// Expect: exactly one notice for the camera prim, primvars-only locator.
TEST(MayaNodesAttributesDirtying, testCameraAiUScaleDirtying)
{
    const std::string cameraShapeFull
        = GetOptionVarOrDefault(kCameraShapeOptionVar, kCameraShapeName);
    if (!PlugExists(cameraShapeFull, "aiUScale")) {
        GTEST_SKIP() << "aiUScale not found on camera shape (Arnold attributes missing)";
    }
    const std::string shapeNamePart = GetShapeNameFromFullPath(cameraShapeFull);
    const std::string parentNamePart = GetParentNameFromFullPath(cameraShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaSceneIndexForShape(sceneIndices, shapeNamePart);
    ASSERT_TRUE(mayaSceneIndex) << "Camera prim not found in MayaHydraSceneIndex";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_TRUE(SetAttrAndRefreshValue(cameraShapeFull, "aiUScale", "0.5"));

    const DirtyNoticeSummary summary = SummarizeDirtyNoticesSince(
        notifsAccumulator,
        startIndex,
        shapeNamePart,
        parentNamePart,
        /*allowExtCompPrimvars*/ false);

    ExpectSingleNoticeCommon(
        summary,
        notifsAccumulator,
        startIndex,
        "aiUScale",
        "Camera aiUScale dirty notices should be primvars only");
    EXPECT_EQ(summary.primvarsCount, 1u)
        << "Expected exactly one primvars notice for aiUScale, got " << summary.primvarsCount;
}

// What: mesh aiUseSubFrame should dirty primvars + extComputationPrimvars.
// How: set aiUseSubFrame on the mesh shape, refresh, then summarize dirtied entries.
// Expect: exactly one notice for the mesh prim, and that single notice contains
//         both primvars and extComputationPrimvars locators.
TEST(MayaNodesAttributesDirtying, testMeshAiUseSubFrameDirtying)
{
    const std::string meshShapeFull = GetOptionVarOrDefault(kMeshShapeOptionVar, kMeshShapeName);
    if (!PlugExists(meshShapeFull, "aiUseSubFrame")) {
        GTEST_SKIP() << "aiUseSubFrame not found on mesh shape (Arnold attributes missing)";
    }
    const std::string shapeNamePart = GetShapeNameFromFullPath(meshShapeFull);
    const std::string parentNamePart = GetParentNameFromFullPath(meshShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaSceneIndexForShape(sceneIndices, shapeNamePart);
    ASSERT_TRUE(mayaSceneIndex) << "Mesh prim not found in MayaHydraSceneIndex";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_TRUE(SetAttrAndRefreshValue(meshShapeFull, "aiUseSubFrame", "1"));

    const DirtyNoticeSummary summary = SummarizeDirtyNoticesSince(
        notifsAccumulator,
        startIndex,
        shapeNamePart,
        parentNamePart,
        /*allowExtCompPrimvars*/ true);

    ExpectSingleNoticeCommon(
        summary,
        notifsAccumulator,
        startIndex,
        "aiUseSubFrame",
        "Mesh aiUseSubFrame dirty notices should be primvars and extComputationPrimvars only");
    EXPECT_EQ(summary.primvarsCount, 1u)
        << "Expected exactly one primvars notice for aiUseSubFrame, got "
        << summary.primvarsCount;
    EXPECT_EQ(summary.extCompPrimvarsCount, 1u)
        << "Expected exactly one extComputationPrimvars notice for aiUseSubFrame, got "
        << summary.extCompPrimvarsCount;
    EXPECT_EQ(summary.combinedPrimvarsExtCompCount, 1u)
        << "Expected a single notice containing both primvars and extComputationPrimvars";
}

// What: light aiShadowDensity (Arnold extension) should dirty only primvars.
// How: set aiShadowDensity on the light shape, refresh, then summarize dirtied entries.
// Expect: exactly one notice for the light prim, primvars-only locator.
TEST(MayaNodesAttributesDirtying, testLightAiShadowDensityDirtying)
{
    const std::string lightShapeFull = GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeName);
    if (!PlugExists(lightShapeFull, "aiShadowDensity")) {
        GTEST_SKIP() << "aiShadowDensity not found on light shape (Arnold attributes missing)";
    }
    const std::string shapeNamePart = GetShapeNameFromFullPath(lightShapeFull);
    const std::string parentNamePart = GetParentNameFromFullPath(lightShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaSceneIndexForShape(sceneIndices, shapeNamePart);
    ASSERT_TRUE(mayaSceneIndex) << "Light prim not found in MayaHydraSceneIndex";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);
    const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();

    ASSERT_TRUE(SetAttrAndRefreshValue(lightShapeFull, "aiShadowDensity", "0.5"));

    const DirtyNoticeSummary summary = SummarizeDirtyNoticesSince(
        notifsAccumulator,
        startIndex,
        shapeNamePart,
        parentNamePart,
        /*allowExtCompPrimvars*/ false);

    ExpectSingleNoticeCommon(
        summary,
        notifsAccumulator,
        startIndex,
        "aiShadowDensity",
        "Light aiShadowDensity dirty notices should be primvars only");
    EXPECT_EQ(summary.primvarsCount, 1u)
        << "Expected exactly one primvars notice for aiShadowDensity, got "
        << summary.primvarsCount;
}

// What: built-in light params (color, intensity) should dirty light params.
// How: set intensity and color on the light shape, refresh, then summarize dirtied entries.
// Expect: exactly one notice per change; light schema with USD's additional locators
//         (primvars/visibility/collections) and no extComputationPrimvars.
TEST(MayaNodesAttributesDirtying, testLightParamDirtying)
{
    const std::string lightShapeFull = GetOptionVarOrDefault(kLightShapeOptionVar, kLightShapeName);
    const std::string shapeNamePart = GetShapeNameFromFullPath(lightShapeFull);
    const std::string parentNamePart = GetParentNameFromFullPath(lightShapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaSceneIndexForShape(sceneIndices, shapeNamePart);
    ASSERT_TRUE(mayaSceneIndex) << "Light prim not found in MayaHydraSceneIndex";

    SceneIndexNotificationsAccumulator notifsAccumulator(mayaSceneIndex);

    auto checkParamsOnly = [&](const char* attrName, const std::string& valueLiteral) {
        const size_t startIndex = notifsAccumulator.GetDirtiedPrimEntries().size();
        ASSERT_TRUE(SetAttrAndRefreshValue(lightShapeFull, attrName, valueLiteral));
        const LightParamNoticeSummary summary = SummarizeLightParamNoticesSince(
            notifsAccumulator, startIndex, shapeNamePart, parentNamePart);
        ExpectLightParamNotice(summary, notifsAccumulator, startIndex, attrName);
    };

    checkParamsOnly("intensity", "2.0");
    checkParamsOnly("color", R"(-type "double3" 0.25 0.5 0.75)");
}
