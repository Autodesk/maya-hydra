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

#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <maya/MGlobal.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnNumericData.h>
#include <maya/MPlug.h>
#include <maya/MString.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

// Unit test: validates that only non-default extension/custom attributes are exposed as Hydra primvars.
namespace {
HdDataSourceLocator primvarsLocator = HdPrimvarsSchema::GetDefaultLocator();

// What: default-valued extension/custom attributes should not emit primvars.
// How: read aiAutobumpVisibility/aiSubdivIterations at default, then set non-default values.
// Expect: default values yield no primvars; non-default values match reference primvar outputs.
TEST(CustomAttributes, defaultArnoldCustomAttributes)
{ 
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndices.front());
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector sceneInspector(mayaSceneIndex);
    PrimEntriesVector foundPrims
        = sceneInspector.FindPrims(CreatePrimPredicate("pCube1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(foundPrims.size(), 1u) << "Mesh prim (pCube1) not found";

    const SdfPath primPath = foundPrims.front().primPath;

    DecimalStreamingOverride decimalStreamingOverride({ PXR_NS::TfDecimalToStringMode::FIXED, 5, false });

    const std::filesystem::path absentReference = getPathToSample("cube_primvar_absent.txt");
    HdDataSourceLocator visLocator = primvarsLocator.Append(TfToken("aiAutobumpVisibility"));
    HdDataSourceLocator iterLocator = primvarsLocator.Append(TfToken("aiSubdivIterations"));

    HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(primPath);
            EXPECT_EQ(prim.primType, HdPrimTypeTokens->mesh);
            ASSERT_NE(prim.dataSource, nullptr);

    // At default: aiAutobumpVisibility and aiSubdivIterations should be absent.
            EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(prim.dataSource, visLocator), absentReference))
        << " See " << getDataSourceComparisonOutputPath(absentReference).string() << " for actual output";
            EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(prim.dataSource, iterLocator), absentReference))
        << " See " << getDataSourceComparisonOutputPath(absentReference).string() << " for actual output";

            MObject cubeNode;
            ASSERT_TRUE(GetDependNodeFromNodeName("pCubeShape1", cubeNode));
    MFnDependencyNode cubeDepNode(cubeNode);
    const MPlug autobumpPlug = cubeDepNode.findPlug("aiAutobumpVisibility", true);
    const MPlug subdivPlug = cubeDepNode.findPlug("aiSubdivIterations", true);
    ASSERT_FALSE(autobumpPlug.isNull());
    ASSERT_FALSE(subdivPlug.isNull());

    const int defaultAutobump = autobumpPlug.asInt();
    const int defaultSubdiv = subdivPlug.asInt();
    const int nonDefaultAutobump = (defaultAutobump == 0) ? 1 : 0;
    const int nonDefaultSubdiv = (defaultSubdiv == 2) ? 3 : 2;
    if (defaultAutobump == nonDefaultAutobump || defaultSubdiv == nonDefaultSubdiv) {
        GTEST_SKIP() << "aiAutobumpVisibility/aiSubdivIterations default matches test non-default.";
    }

    // Set aiAutobumpVisibility to a non-default: should appear as primvar.
    EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiAutobumpVisibility", nonDefaultAutobump));
    MGlobal::executeCommand("refresh");
    prim = mayaSceneIndex->GetPrim(primPath);
            EXPECT_TRUE(dataSourceMatchesReference(
                HdContainerDataSource::Get(prim.dataSource, visLocator),
        getPathToSample("cube_primvar_aiAutobumpVisibility_modified.txt")))
        << " See " << getDataSourceComparisonOutputPath(getPathToSample("cube_primvar_aiAutobumpVisibility_modified.txt")).string() << " for actual output";

    // Set aiSubdivIterations to a non-default: should appear as primvar.
    EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiSubdivIterations", nonDefaultSubdiv));
    MGlobal::executeCommand("refresh");
    prim = mayaSceneIndex->GetPrim(primPath);
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(prim.dataSource, iterLocator),
        getPathToSample("cube_primvar_aiSubdivIterations_modified.txt")))
        << " See " << getDataSourceComparisonOutputPath(getPathToSample("cube_primvar_aiSubdivIterations_modified.txt")).string() << " for actual output";
}

// What: camera compound attributes should only emit primvars when non-default.
// How: locate a camera prim, verify aiLookAt is absent at default, then set a non-default value.
// Expect: default (0,0,0) has no primvar; non-default value produces the primvar output.
TEST(CustomAttributes, defaultArnoldCameraCompoundAttributes)
{
#ifdef CONFIGURABLE_DECIMAL_STREAMING_AVAILABLE
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndices.front());
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector sceneInspector(mayaSceneIndex);
    PrimEntriesVector foundPrims
        = sceneInspector.FindPrims(CreatePrimPredicate("perspShape", HdPrimTypeTokens->camera), 1);
    if (foundPrims.empty()) {
        foundPrims = sceneInspector.FindPrims(CreatePrimPredicate("cameraShape1", HdPrimTypeTokens->camera), 1);
    }
    ASSERT_EQ(foundPrims.size(), 1u) << "Camera prim (perspShape/cameraShape1) not found";

    const SdfPath primPath = foundPrims.front().primPath;

    DecimalStreamingOverride decimalStreamingOverride({ PXR_NS::TfDecimalToStringMode::FIXED, 5, false });

    const std::filesystem::path absentReference = getPathToSample("cube_primvar_absent.txt");
    HdDataSourceLocator lookAtLocator = primvarsLocator.Append(TfToken("aiLookAt"));

    HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(primPath);
    ASSERT_EQ(prim.primType, HdPrimTypeTokens->camera);
    ASSERT_NE(prim.dataSource, nullptr);

    // At default (0,0,0): aiLookAt should be absent.
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(prim.dataSource, lookAtLocator), absentReference))
        << " See " << getDataSourceComparisonOutputPath(absentReference).string() << " for actual";

    // Set aiLookAt to (1, 0, 0) (non-default): should appear as primvar.
    // aiLookAt is a multi attribute; elements must be set individually via MPlug.
    MObject cameraNode;
    ASSERT_TRUE(GetDependNodeFromNodeName(primPath.GetName().c_str(), cameraNode));
    MFnDependencyNode depNode(cameraNode);
    MPlug lookAtPlug = depNode.findPlug("aiLookAt", true);
    ASSERT_FALSE(lookAtPlug.isNull());

    // aiLookAt can be either a plain compound or an array/compound.
    MPlug elementPlug = lookAtPlug;
    if (lookAtPlug.isArray()) {
        if (lookAtPlug.numElements() == 0) {
            lookAtPlug.setNumElements(1);
        }
        elementPlug = lookAtPlug.elementByLogicalIndex(0);
    }

    if (elementPlug.numChildren() >= 3) {
        // Use setAttr to ensure the change is recorded for callbacks and evaluation.
        MStatus numericStatus;
        MFnNumericAttribute firstChild(elementPlug.child(0).attribute(), &numericStatus);
        if (!numericStatus) {
            GTEST_SKIP() << "aiLookAt child attribute is not numeric.";
        }
        const MString typeName
            = (firstChild.unitType() == MFnNumericData::kFloat) ? "float3" : "double3";
        const MString attrName = elementPlug.name();
        const MString cmd = "setAttr -type \"" + typeName + "\" \"" + attrName + "\" 0 0 0";
        ASSERT_EQ(MGlobal::executeCommand(cmd), MStatus::kSuccess);

        const double x = elementPlug.child(0).asDouble();
        const double y = elementPlug.child(1).asDouble();
        const double z = elementPlug.child(2).asDouble();
        if (std::abs(x) > 1e-9 || std::abs(y) > 1e-9 || std::abs(z) > 1e-9) {
            GTEST_SKIP() << "aiLookAt could not be set (current value is " << x << ", " << y
                         << ", " << z << ").";
        }
    } else {
        GTEST_SKIP() << "aiLookAt structure not as expected (compound with 3 children).";
    }
    MGlobal::executeCommand("refresh");

    prim = mayaSceneIndex->GetPrim(primPath);
    const auto lookAtRef = getPathToSample("camera_primvar_aiLookAt_modified.txt");
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(prim.dataSource, lookAtLocator), lookAtRef))
        << " See " << getDataSourceComparisonOutputPath(lookAtRef).string() << " for actual";
#else
    GTEST_SKIP() << "Skipping test, configurable decimal streaming is unavailable.";
#endif
}

// What: primvars appear for non-default ai* attrs and disappear when reset.
// How: set and reset ai* attributes on mesh, camera, and light, then inspect data sources.
// Expect: primvars appear on non-default values and are absent after reset to defaults.
TEST(CustomAttributes, aiPrimvarsAppearAndRemovedWhenReset)
{
#ifdef CONFIGURABLE_DECIMAL_STREAMING_AVAILABLE
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);

    auto mayaSceneIndex = FindMayaHydraSceneIndex(sceneIndices.front());
    ASSERT_TRUE(mayaSceneIndex) << "MayaHydraSceneIndex not found in scene index tree";

    SceneIndexInspector sceneInspector(mayaSceneIndex);

    DecimalStreamingOverride decimalStreamingOverride({ PXR_NS::TfDecimalToStringMode::FIXED, 5, false });

    const std::filesystem::path absentReference = getPathToSample("cube_primvar_absent.txt");

    // --- Mesh (pCube1): aiSubdivIterations, aiAutobumpVisibility ---
    PrimEntriesVector meshPrims = sceneInspector.FindPrims(CreatePrimPredicate("pCube1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(meshPrims.size(), 1u) << "Mesh prim (pCube1) not found";
    {
        const SdfPath primPath = meshPrims.front().primPath;
        MObject cubeNode;
        ASSERT_TRUE(GetDependNodeFromNodeName("pCubeShape1", cubeNode));
        MFnDependencyNode cubeDepNode(cubeNode);
        const MPlug autobumpPlug = cubeDepNode.findPlug("aiAutobumpVisibility", true);
        const MPlug subdivPlug = cubeDepNode.findPlug("aiSubdivIterations", true);
        ASSERT_FALSE(autobumpPlug.isNull());
        ASSERT_FALSE(subdivPlug.isNull());

        const int defaultAutobump = autobumpPlug.asInt();
        const int defaultSubdiv = subdivPlug.asInt();
        const int nonDefaultAutobump = (defaultAutobump == 0) ? 1 : 0;
        const int nonDefaultSubdiv = (defaultSubdiv == 2) ? 3 : 2;
        if (defaultAutobump == nonDefaultAutobump || defaultSubdiv == nonDefaultSubdiv) {
            GTEST_SKIP() << "aiAutobumpVisibility/aiSubdivIterations default matches test non-default.";
        }

        // Set non-default: primvars appear
        EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiSubdivIterations", nonDefaultSubdiv));
        EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiAutobumpVisibility", nonDefaultAutobump));
        MGlobal::executeCommand("refresh");
        HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(primPath);
        const auto subdivRef = getPathToSample("cube_primvar_aiSubdivIterations_modified.txt");
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiSubdivIterations"))),
            subdivRef))
            << "Mesh aiSubdivIterations primvar should appear. See "
            << getDataSourceComparisonOutputPath(subdivRef).string() << " for actual";
        const auto autobumpRef = getPathToSample("cube_primvar_aiAutobumpVisibility_modified.txt");
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiAutobumpVisibility"))),
            autobumpRef))
            << "Mesh aiAutobumpVisibility primvar should appear. See "
            << getDataSourceComparisonOutputPath(autobumpRef).string() << " for actual";

        // Reset to default: primvars removed
        EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiSubdivIterations", defaultSubdiv));
        EXPECT_TRUE(SetNodeAttribute(cubeNode, "aiAutobumpVisibility", defaultAutobump));
        MGlobal::executeCommand("refresh");
        prim = mayaSceneIndex->GetPrim(primPath);
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiSubdivIterations"))),
            absentReference))
            << "Mesh aiSubdivIterations primvar should be removed when reset. See "
            << getDataSourceComparisonOutputPath(absentReference).string() << " for actual";
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiAutobumpVisibility"))),
            absentReference))
            << "Mesh aiAutobumpVisibility primvar should be removed when reset. See "
            << getDataSourceComparisonOutputPath(absentReference).string() << " for actual";
    }

    // --- Camera (perspShape): aiUScale ---
    PrimEntriesVector camPrims
        = sceneInspector.FindPrims(CreatePrimPredicate("perspShape", HdPrimTypeTokens->camera), 1);
    if (camPrims.empty()) {
        camPrims = sceneInspector.FindPrims(CreatePrimPredicate("cameraShape1", HdPrimTypeTokens->camera), 1);
    }
    ASSERT_EQ(camPrims.size(), 1u) << "Camera prim (perspShape/cameraShape1) not found";
    {
        const SdfPath primPath = camPrims.front().primPath;
        MObject cameraNode;
        ASSERT_TRUE(GetDependNodeFromNodeName(primPath.GetName().c_str(), cameraNode));

        // Set non-default: primvar appears
        EXPECT_TRUE(SetNodeAttribute(cameraNode, "aiUScale", 2.0));
        MGlobal::executeCommand("refresh");
        HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(primPath);
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiUScale"))),
            getPathToSample("camera_primvar_aiUScale_modified.txt")))
            << "Camera aiUScale primvar should appear";

        // Reset to default: primvar removed
        EXPECT_TRUE(SetNodeAttribute(cameraNode, "aiUScale", 1.0));
        MGlobal::executeCommand("refresh");
        prim = mayaSceneIndex->GetPrim(primPath);
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiUScale"))),
            absentReference))
            << "Camera aiUScale primvar should be removed when reset";
    }

    // --- Light (aiSkyDomeLightShape1): aiColorTemperature ---
    PrimEntriesVector lightPrims
        = sceneInspector.FindPrims(CreatePrimPredicate("aiSkyDomeLightShape1", HdPrimTypeTokens->domeLight), 1);
    ASSERT_EQ(lightPrims.size(), 1u)
        << "Light prim (aiSkyDomeLightShape1) not found - ensure setupSceneWithLight created it";
    {
        const SdfPath primPath = lightPrims.front().primPath;
        MObject lightNode;
        ASSERT_TRUE(GetDependNodeFromNodeName("aiSkyDomeLightShape1", lightNode));

        // Set non-default (2000, default is 6500): primvar appears
        EXPECT_TRUE(SetNodeAttribute(lightNode, "aiColorTemperature", 2000.0f));
        MGlobal::executeCommand("refresh");
        HdSceneIndexPrim prim = mayaSceneIndex->GetPrim(primPath);
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiColorTemperature"))),
            getPathToSample("light_primvar_aiColorTemperature_modified.txt")))
            << "Light aiColorTemperature primvar should appear";

        // Reset to default (6500): primvar removed
        EXPECT_TRUE(SetNodeAttribute(lightNode, "aiColorTemperature", 6500.0f));
        MGlobal::executeCommand("refresh");
        prim = mayaSceneIndex->GetPrim(primPath);
        EXPECT_TRUE(dataSourceMatchesReference(
            HdContainerDataSource::Get(prim.dataSource, primvarsLocator.Append(TfToken("aiColorTemperature"))),
            absentReference))
            << "Light aiColorTemperature primvar should be removed when reset";
    }
#else
    GTEST_SKIP() << "Skipping test, configurable decimal streaming is unavailable.";
#endif
}

}
