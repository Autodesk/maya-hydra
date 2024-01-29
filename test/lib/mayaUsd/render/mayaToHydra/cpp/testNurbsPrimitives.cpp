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

#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <maya/MFnDependencyNode.h>
#include <maya/MFnMesh.h>
#include <maya/MFnNurbsSurface.h>
#include <maya/MObject.h>
#include <maya/MPlug.h>
#include <maya/MPxNode.h>
#include <maya/MViewport2Renderer.h>
#include <maya/M3dView.h>

#include <gtest/gtest.h>

#include <sstream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <iterator>
#include <optional>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

HdDataSourceLocator topologyLocator = HdMeshSchema::GetTopologyLocator();
HdDataSourceLocator pointsLocator = HdPrimvarsSchema::GetPointsLocator();

template <typename AttrType> bool setAttribute(MObject node, std::string attrName, AttrType newValue)
{
    MStatus           dependencyNodeStatus;
    MFnDependencyNode dependencyNode(node, &dependencyNodeStatus);
    if (!dependencyNodeStatus) {
        return false;
    }
    MStatus plugStatus;
    MPlug   plug = dependencyNode.findPlug(attrName.c_str(), true, &plugStatus);
    if (!plugStatus) {
        return false;
    }
    return plug.setValue(newValue);
}

FindPrimPredicate getNurbPrimPredicate(const std::string& nurbShapeName, const TfToken& primType)
{
    return [nurbShapeName,
            primType](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        if (primPath.GetParentPath().GetElementString() != nurbShapeName) {
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

    PrimEntriesVector foundPrims = inspector.FindPrims(getNurbPrimPredicate("nurbsTorusShape1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim meshPrim = foundPrims.front().prim;
    EXPECT_TRUE(meshPrim.primType != TfToken());
    ASSERT_TRUE(meshPrim.dataSource != nullptr);

    HdDataSourceBaseHandle topologyDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, topologyLocator);
    HdDataSourceBaseHandle pointsDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, pointsLocator);

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topologyDataSource_fresh.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        pointsDataSource, getPathToSample("torus_pointsDataSource_fresh.txt")));

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbTorus1", makeNurbNode));
    EXPECT_TRUE(setAttribute(makeNurbNode, "startSweep", 50));
    EXPECT_TRUE(setAttribute(makeNurbNode, "endSweep", 300));
    EXPECT_TRUE(setAttribute(makeNurbNode, "radius", 2));
    EXPECT_TRUE(setAttribute(makeNurbNode, "degree", 1));
    EXPECT_TRUE(setAttribute(makeNurbNode, "sections", 12));
    EXPECT_TRUE(setAttribute(makeNurbNode, "spans", 6));
    EXPECT_TRUE(setAttribute(makeNurbNode, "heightRatio", 0.8f));
    EXPECT_TRUE(setAttribute(makeNurbNode, "minorSweep", 250));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topologyDataSource_modified.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        pointsDataSource, getPathToSample("torus_pointsDataSource_modified.txt")));

    EXPECT_TRUE(setAttribute(makeNurbNode, "useTolerance", true));
    EXPECT_TRUE(setAttribute(makeNurbNode, "tolerance", 0.05f));
    EXPECT_TRUE(M3dView::active3dView().refresh());

    EXPECT_TRUE(dataSourceMatchesReference(
        topologyDataSource, getPathToSample("torus_topologyDataSource_tolerance.txt")));
    EXPECT_TRUE(dataSourceMatchesReference(
        pointsDataSource, getPathToSample("torus_pointsDataSource_tolerance.txt")));
}
