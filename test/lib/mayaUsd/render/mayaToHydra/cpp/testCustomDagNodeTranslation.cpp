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
//
// C++ GTest tests for the custom plugin DAG node translation feature.
// Validates that unrecognized Maya plugin nodes (using aiPhotometricLight as
// the concrete example) are correctly translated to Hydra as mayaCustomDagNode
// prims with the right type name, non-default attributes, per-attribute dirty
// locators, and that nodes with all-default attributes have an empty
// mayaAttributes dictionary.
//
// The Python counterpart (testCustomDagNodeTranslation.py) sets up the Maya
// scene and passes node paths as positional arguments to mayaHydraCppTest.
//

#include "testUtils.h"

#include <mayaHydraLib/adapters/tokens.h>

#include <pxr/base/tf/token.h>
#include <pxr/base/vt/dictionary.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/MGlobal.h>

#include <gtest/gtest.h>

#include <cmath>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE
using namespace MayaHydra;

// Verify prim type is mayaCustomDagNode and data source contains expected values.
TEST(CustomDagNodeTranslation, verifyPrimTypeAndDataSource)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim)
        << "mayaCustomDagNode prim for '" << shapeNamePart << "' not found in any scene index";

    SceneIndexInspector inspector(siWithPrim);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);

    HdSceneIndexPrim prim = found.front().prim;
    ASSERT_EQ(prim.primType, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_NE(prim.dataSource, nullptr);

    // Read mayaNode container
    auto mayaNodeDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(MayaHydraAdapterTokens->mayaNode));
    ASSERT_TRUE(mayaNodeDs) << "mayaNode data source not found on prim";

    // Verify mayaTypeName
    auto typeNameDs = HdTypedSampledDataSource<TfToken>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaTypeName));
    ASSERT_TRUE(typeNameDs) << "mayaTypeName data source not found";
    EXPECT_EQ(typeNameDs->GetTypedValue(0.0f), TfToken("aiPhotometricLight"));

    // Verify mayaAttributes dictionary
    auto attrsDs = HdTypedSampledDataSource<VtDictionary>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaAttributes));
    ASSERT_TRUE(attrsDs) << "mayaAttributes data source not found";
    VtDictionary attrs = attrsDs->GetTypedValue(0.0f);

    // Check intensity = 2.5
    auto itIntensity = attrs.find("intensity");
    ASSERT_NE(itIntensity, attrs.end()) << "'intensity' not in mayaAttributes";
    float intensityVal = 0.0f;
    if (itIntensity->second.IsHolding<float>()) {
        intensityVal = itIntensity->second.UncheckedGet<float>();
    } else if (itIntensity->second.IsHolding<double>()) {
        intensityVal = static_cast<float>(itIntensity->second.UncheckedGet<double>());
    }
    EXPECT_NEAR(intensityVal, 2.5f, 1e-5f);

    // Check aiExposure = 3.0
    auto itExposure = attrs.find("aiExposure");
    ASSERT_NE(itExposure, attrs.end()) << "'aiExposure' not in mayaAttributes";
    float exposureVal = 0.0f;
    if (itExposure->second.IsHolding<float>()) {
        exposureVal = itExposure->second.UncheckedGet<float>();
    } else if (itExposure->second.IsHolding<double>()) {
        exposureVal = static_cast<float>(itExposure->second.UncheckedGet<double>());
    }
    EXPECT_NEAR(exposureVal, 3.0f, 1e-5f);

    // Check aiFilename = "/path/to/test.ies"
    auto itFilename = attrs.find("aiFilename");
    ASSERT_NE(itFilename, attrs.end()) << "'aiFilename' not in mayaAttributes";
    ASSERT_TRUE(itFilename->second.IsHolding<std::string>());
    EXPECT_EQ(itFilename->second.UncheckedGet<std::string>(), "/path/to/test.ies");
}

// Verify that changing an attribute produces the correct per-attribute dirty notice
// and the updated value is readable.
TEST(CustomDagNodeTranslation, verifyAttributeDirtyAndUpdate)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim);

    auto mayaSI = FindMayaHydraSceneIndex(siWithPrim);
    ASSERT_TRUE(mayaSI) << "MayaHydraSceneIndex not found";

    SceneIndexInspector inspector(mayaSI);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);
    const SdfPath primPath = found.front().primPath;

    SceneIndexNotificationsAccumulator accumulator(mayaSI);
    const size_t startIndex = accumulator.GetDirtiedPrimEntries().size();

    MGlobal::executeCommand(
        ("setAttr \"" + shapeFull + ".intensity\" 7.0").c_str());
    MGlobal::executeCommand("refresh");

    // Verify dirty notice for intensity attribute
    const HdDataSourceLocator intensityLocator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        TfToken("intensity"));

    bool foundDirty = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath == primPath
            && entries[i].dirtyLocators.Intersects(intensityLocator)) {
            foundDirty = true;
            break;
        }
    }
    EXPECT_TRUE(foundDirty)
        << "Expected dirty notice with per-attribute locator for 'intensity'";

    // Verify the updated value
    HdSceneIndexPrim updatedPrim = mayaSI->GetPrim(primPath);
    ASSERT_NE(updatedPrim.dataSource, nullptr);
    auto mayaNodeDs = HdContainerDataSource::Cast(
        updatedPrim.dataSource->Get(MayaHydraAdapterTokens->mayaNode));
    ASSERT_TRUE(mayaNodeDs);
    auto attrsDs = HdTypedSampledDataSource<VtDictionary>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaAttributes));
    ASSERT_TRUE(attrsDs);
    VtDictionary attrs = attrsDs->GetTypedValue(0.0f);

    auto itIntensity = attrs.find("intensity");
    ASSERT_NE(itIntensity, attrs.end());
    float intensityVal = 0.0f;
    if (itIntensity->second.IsHolding<float>()) {
        intensityVal = itIntensity->second.UncheckedGet<float>();
    } else if (itIntensity->second.IsHolding<double>()) {
        intensityVal = static_cast<float>(itIntensity->second.UncheckedGet<double>());
    }
    EXPECT_NEAR(intensityVal, 7.0f, 1e-5f);
}

// Verify that setting one attribute does not produce duplicate dirty notices
// and does not dirty other attributes.
TEST(CustomDagNodeTranslation, verifyNoDuplicateDirtyNotices)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim);

    auto mayaSI = FindMayaHydraSceneIndex(siWithPrim);
    ASSERT_TRUE(mayaSI) << "MayaHydraSceneIndex not found";

    SceneIndexInspector inspector(mayaSI);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);
    const SdfPath primPath = found.front().primPath;

    SceneIndexNotificationsAccumulator accumulator(mayaSI);
    const size_t startIndex = accumulator.GetDirtiedPrimEntries().size();

    MGlobal::executeCommand(
        ("setAttr \"" + shapeFull + ".aiExposure\" 5.0").c_str());
    MGlobal::executeCommand("refresh");

    const HdDataSourceLocator exposureLocator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        TfToken("aiExposure"));

    const HdDataSourceLocator intensityLocator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        TfToken("intensity"));

    size_t exposureCount = 0;
    bool   intensityDirtied = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != primPath) {
            continue;
        }
        if (entries[i].dirtyLocators.Intersects(exposureLocator)) {
            ++exposureCount;
        }
        if (entries[i].dirtyLocators.Intersects(intensityLocator)) {
            intensityDirtied = true;
        }
    }

    EXPECT_EQ(exposureCount, 1u)
        << "Expected exactly one dirty notice for aiExposure, got " << exposureCount;
    EXPECT_FALSE(intensityDirtied)
        << "Changing aiExposure should NOT dirty the intensity locator";
}

// Verify that changing a plugin attribute does NOT produce a spurious xform
// dirty notice. Only the per-attribute locator should be dirtied.
TEST(CustomDagNodeTranslation, verifyNoSpuriousXformDirty)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim);

    auto mayaSI = FindMayaHydraSceneIndex(siWithPrim);
    ASSERT_TRUE(mayaSI) << "MayaHydraSceneIndex not found";

    SceneIndexInspector inspector(mayaSI);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);
    const SdfPath primPath = found.front().primPath;

    SceneIndexNotificationsAccumulator accumulator(mayaSI);
    const size_t startIndex = accumulator.GetDirtiedPrimEntries().size();

    MGlobal::executeCommand(
        ("setAttr \"" + shapeFull + ".intensity\" 9.0").c_str());
    MGlobal::executeCommand("refresh");

    const HdDataSourceLocator xformLocator = HdXformSchema::GetDefaultLocator();
    const HdDataSourceLocator intensityLocator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        TfToken("intensity"));

    bool xformDirtied = false;
    bool intensityDirtied = false;
    const auto& entries = accumulator.GetDirtiedPrimEntries();
    for (size_t i = startIndex; i < entries.size(); ++i) {
        if (entries[i].primPath != primPath) {
            continue;
        }
        if (entries[i].dirtyLocators.Intersects(xformLocator)) {
            xformDirtied = true;
        }
        if (entries[i].dirtyLocators.Intersects(intensityLocator)) {
            intensityDirtied = true;
        }
    }

    EXPECT_TRUE(intensityDirtied)
        << "Expected dirty notice for intensity attribute";
    EXPECT_FALSE(xformDirtied)
        << "Changing a plugin attribute should NOT dirty the xform locator";
}

// Verify that attributes left at their default value are NOT included in mayaAttributes.
TEST(CustomDagNodeTranslation, verifyDefaultValuesNotTranslated)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim)
        << "mayaCustomDagNode prim for '" << shapeNamePart << "' not found";

    SceneIndexInspector inspector(siWithPrim);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);

    HdSceneIndexPrim prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    auto mayaNodeDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(MayaHydraAdapterTokens->mayaNode));
    ASSERT_TRUE(mayaNodeDs);

    auto attrsDs = HdTypedSampledDataSource<VtDictionary>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaAttributes));
    ASSERT_TRUE(attrsDs);
    VtDictionary attrs = attrsDs->GetTypedValue(0.0f);

    // Non-default attributes we explicitly set must be present.
    EXPECT_NE(attrs.find("intensity"), attrs.end())
        << "Non-default 'intensity' should be in mayaAttributes";
    EXPECT_NE(attrs.find("aiExposure"), attrs.end())
        << "Non-default 'aiExposure' should be in mayaAttributes";
    EXPECT_NE(attrs.find("aiFilename"), attrs.end())
        << "Non-default 'aiFilename' should be in mayaAttributes";

    // Attributes we did NOT modify should remain at their defaults and be excluded.
    // decayRate (short, default 2) and dropoff (double, default 2.0) are static
    // attributes on the light that we never changed.
    EXPECT_EQ(attrs.find("decayRate"), attrs.end())
        << "Default-valued 'decayRate' should NOT be in mayaAttributes";
    EXPECT_EQ(attrs.find("dropoff"), attrs.end())
        << "Default-valued 'dropoff' should NOT be in mayaAttributes";
}

// Verify that a plugin node with ALL attributes at default is still added to
// Hydra (so attribute changes are tracked from creation), but its mayaAttributes
// dictionary is empty.
TEST(CustomDagNodeTranslation, verifyDefaultOnlyNodeInHydraWithEmptyAttrs)
{
    auto [argc, argv] = getTestingArgs();
    ASSERT_GE(argc, 1);
    const std::string shapeFull(argv[0]);
    const std::string shapeNamePart = GetShapeNameFromFullPath(shapeFull);

    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    HdSceneIndexBaseRefPtr siWithPrim = FindTerminalSceneIndexWithPrim(
        sceneIndices, shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode);
    ASSERT_TRUE(siWithPrim)
        << "Plugin node '" << shapeNamePart << "' should be in Hydra even with all default attributes";

    SceneIndexInspector inspector(siWithPrim);
    PrimEntriesVector   found = inspector.FindPrims(
        CreatePrimPredicate(shapeNamePart, MayaHydraAdapterTokens->mayaCustomDagNode), 1);
    ASSERT_GE(found.size(), 1u);

    HdSceneIndexPrim prim = found.front().prim;
    ASSERT_NE(prim.dataSource, nullptr);

    auto mayaNodeDs = HdContainerDataSource::Cast(
        prim.dataSource->Get(MayaHydraAdapterTokens->mayaNode));
    ASSERT_TRUE(mayaNodeDs);

    auto typeNameDs = HdTypedSampledDataSource<TfToken>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaTypeName));
    ASSERT_TRUE(typeNameDs);
    EXPECT_EQ(typeNameDs->GetTypedValue(0.0f), TfToken("aiPhotometricLight"));

    auto attrsDs = HdTypedSampledDataSource<VtDictionary>::Cast(
        mayaNodeDs->Get(MayaHydraAdapterTokens->mayaAttributes));
    ASSERT_TRUE(attrsDs);
    VtDictionary attrs = attrsDs->GetTypedValue(0.0f);
    EXPECT_TRUE(attrs.empty())
        << "Node with all default attributes should have an empty mayaAttributes dictionary";
}
