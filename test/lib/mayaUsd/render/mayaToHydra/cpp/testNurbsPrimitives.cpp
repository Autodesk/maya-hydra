// Copyright 2024 Autodesk
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

#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/M3dView.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MViewport2Renderer.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <iterator>
#include <optional>
#include <sstream>
#include <string>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

HdDataSourceLocator topologyLocator = HdMeshSchema::GetTopologyLocator();
HdDataSourceLocator pointsLocator = HdPrimvarsSchema::GetPointsLocator();

FindPrimPredicate getNurbPrimPredicate(const std::string& nurbsName, const TfToken& primType)
{
    return [nurbsName,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetAsString().find(nurbsName) == std::string::npos) {
            return false;
        }
        HdSceneIndexPrim prim = sceneIndex->GetPrim(primPath);
        return prim.primType == primType;
    };
}

} // namespace

TEST(NurbsPrimitives, nurbsTorus)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector foundPrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsTorus1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim meshPrim = foundPrims.front().prim;
    EXPECT_TRUE(meshPrim.primType != TfToken());
    ASSERT_TRUE(meshPrim.dataSource != nullptr);

    HdDataSourceBaseHandle topologyDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, topologyLocator);
    HdDataSourceBaseHandle pointsDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, pointsLocator);

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topology_fresh.txt")));
    EXPECT_TRUE(
        dataSourceMatchesReference(pointsDataSource, getPathToSample("torus_points_fresh.txt")));

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbTorus1", makeNurbNode));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "startSweep", 50));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "endSweep", 300));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "radius", 2));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "sections", 12));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "spans", 6));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "heightRatio", 0.8f));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "minorSweep", 250));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topology_modified.txt")));
    EXPECT_TRUE(
        dataSourceMatchesReference(pointsDataSource, getPathToSample("torus_points_modified.txt")));

    EXPECT_TRUE(SetAttribute(makeNurbNode, "useTolerance", true));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "tolerance", 0.05f));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topology_tolerance.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        pointsDataSource, getPathToSample("torus_points_tolerance.txt")));
}

TEST(NurbsPrimitives, nurbsCube)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector planePrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsCube1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(planePrims.size(), 6u);

    auto testPlanePrims = [planePrims](std::string testSuffix) -> void {
        for (PrimEntry planePrim : planePrims) {
            EXPECT_TRUE(planePrim.prim.primType != TfToken());
            ASSERT_TRUE(planePrim.prim.dataSource != nullptr);
            HdDataSourceBaseHandle topologyDataSource
                = HdContainerDataSource::Get(planePrim.prim.dataSource, topologyLocator);
            HdDataSourceBaseHandle pointsDataSource
                = HdContainerDataSource::Get(planePrim.prim.dataSource, pointsLocator);
            EXPECT_TRUE(dataSourceMatchesReference(
                topologyDataSource,
                getPathToSample(
                    "cube_" + planePrim.primPath.GetParentPath().GetElementString() + "_topology_"
                    + testSuffix + ".txt")));
            EXPECT_TRUE(dataSourceMatchesReference(
                pointsDataSource,
                getPathToSample(
                    "cube_" + planePrim.primPath.GetParentPath().GetElementString() + "_points_"
                    + testSuffix + ".txt")));
        }
    };

    testPlanePrims("fresh");

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbCube1", makeNurbNode));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "patchesU", 2));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "patchesV", 3));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "width", 4));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "lengthRatio", 5));
    EXPECT_TRUE(SetAttribute(makeNurbNode, "heightRatio", 6));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    testPlanePrims("modified");
}
