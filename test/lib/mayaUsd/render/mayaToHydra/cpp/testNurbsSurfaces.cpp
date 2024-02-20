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

#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/M3dView.h>

#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

HdDataSourceLocator curvesTopologyLocator = HdBasisCurvesSchema::GetTopologyLocator();
HdDataSourceLocator meshTopologyLocator = HdMeshSchema::GetTopologyLocator();
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

TEST(NurbsSurfaces, nurbsTorus)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector foundPrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsTorus1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim torusPrim = foundPrims.front().prim;
    EXPECT_EQ(torusPrim.primType, HdPrimTypeTokens->mesh);
    ASSERT_NE(torusPrim.dataSource, nullptr);

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, meshTopologyLocator),
        getPathToSample("torus_topology_fresh.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, pointsLocator),
        getPathToSample("torus_points_fresh.txt")));

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbTorus1", makeNurbNode));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "startSweep", 50));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "endSweep", 300));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "radius", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "sections", 12));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "spans", 6));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "heightRatio", 0.8f));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "minorSweep", 250));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, meshTopologyLocator),
        getPathToSample("torus_topology_modified.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, pointsLocator),
        getPathToSample("torus_points_modified.txt")));

    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "useTolerance", true));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "tolerance", 0.05f));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, meshTopologyLocator),
        getPathToSample("torus_topology_tolerance.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(torusPrim.dataSource, pointsLocator),
        getPathToSample("torus_points_tolerance.txt")));
}

TEST(NurbsSurfaces, nurbsCube)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector planePrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsCube1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(planePrims.size(), 6u);

    auto testPlanePrims = [planePrims](std::string testSuffix) -> void {
        for (PrimEntry planePrim : planePrims) {
            EXPECT_EQ(planePrim.prim.primType, HdPrimTypeTokens->mesh);
            ASSERT_NE(planePrim.prim.dataSource, nullptr);
            EXPECT_TRUE(dataSourceMatchesReference(
                HdContainerDataSource::Get(planePrim.prim.dataSource, meshTopologyLocator),
                getPathToSample(
                    "cube_" + planePrim.primPath.GetParentPath().GetElementString() + "_topology_"
                    + testSuffix + ".txt")));
            EXPECT_TRUE(dataSourceMatchesReference(
                HdContainerDataSource::Get(planePrim.prim.dataSource, pointsLocator),
                getPathToSample(
                    "cube_" + planePrim.primPath.GetParentPath().GetElementString() + "_points_"
                    + testSuffix + ".txt")));
        }
    };

    testPlanePrims("fresh");

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbCube1", makeNurbNode));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "patchesU", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "patchesV", 3));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "width", 4));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "lengthRatio", 5));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "heightRatio", 6));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    testPlanePrims("modified");
}

TEST(NurbsSurfaces, nurbsCircle)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector foundPrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsCircle1", HdPrimTypeTokens->basisCurves));
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim circlePrim = foundPrims.front().prim;
    EXPECT_EQ(circlePrim.primType, HdPrimTypeTokens->basisCurves);
    ASSERT_NE(circlePrim.dataSource, nullptr);

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, curvesTopologyLocator),
        getPathToSample("circle_topology_fresh.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, pointsLocator),
        getPathToSample("circle_points_fresh.txt")));

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbCircle1", makeNurbNode));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "sweep", 180));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "radius", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "sections", 12));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalX", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalY", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalZ", 3));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerX", 4));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerY", 5));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerZ", 6));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "firstPointX", 7));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "firstPointY", 8));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "firstPointZ", 9));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, curvesTopologyLocator),
        getPathToSample("circle_topology_modified.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, pointsLocator),
        getPathToSample("circle_points_modified.txt")));

    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "useTolerance", true));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "tolerance", 0.05f));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, curvesTopologyLocator),
        getPathToSample("circle_topology_tolerance.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, pointsLocator),
        getPathToSample("circle_points_tolerance.txt")));

    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "fixCenter", false));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, curvesTopologyLocator),
        getPathToSample("circle_topology_unfixedCenter.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        HdContainerDataSource::Get(circlePrim.dataSource, pointsLocator),
        getPathToSample("circle_points_unfixedCenter.txt")));
}

TEST(NurbsSurfaces, nurbsSquare)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector linePrims
        = inspector.FindPrims(getNurbPrimPredicate("nurbsSquare1", HdPrimTypeTokens->basisCurves));
    ASSERT_EQ(linePrims.size(), 4u);

    auto testLinePrims = [linePrims](std::string testSuffix) -> void {
        for (PrimEntry linePrim : linePrims) {
            EXPECT_EQ(linePrim.prim.primType, HdPrimTypeTokens->basisCurves);
            ASSERT_NE(linePrim.prim.dataSource, nullptr);
            EXPECT_TRUE(dataSourceMatchesReference(
                HdContainerDataSource::Get(linePrim.prim.dataSource, curvesTopologyLocator),
                getPathToSample(
                    "square_" + linePrim.primPath.GetParentPath().GetElementString() + "_topology_"
                    + testSuffix + ".txt")));
            EXPECT_TRUE(dataSourceMatchesReference(
                HdContainerDataSource::Get(linePrim.prim.dataSource, pointsLocator),
                getPathToSample(
                    "square_" + linePrim.primPath.GetParentPath().GetElementString() + "_points_"
                    + testSuffix + ".txt")));
        }
    };

    testLinePrims("fresh");

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbsSquare1", makeNurbNode));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "sideLength1", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "sideLength2", 3));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "spansPerSide", 4));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalX", 1));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalY", 2));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "normalZ", 3));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerX", 4));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerY", 5));
    EXPECT_TRUE(SetNodeAttribute(makeNurbNode, "centerZ", 6));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    testLinePrims("modified");
}
