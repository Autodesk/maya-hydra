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

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

namespace {

HdDataSourceLocator topologyLocator = HdMeshSchema::GetTopologyLocator();
HdDataSourceLocator pointsLocator = HdPrimvarsSchema::GetPointsLocator();

class NurbMaker
{
public:
    NurbMaker(MFnDependencyNode& makeNurbNode)
        : _makeNurbNode(makeNurbNode)
    {
    }

    template <typename AttrType> bool setAttribute(std::string attrName, AttrType newValue)
    {
        MStatus findPlugStatus;
        MPlug   plug = _makeNurbNode.findPlug(MString(attrName.c_str()), true, &findPlugStatus);
        if (!findPlugStatus) {
            return false;
        }
        return plug.setValue(newValue);
    }

private:
    MFnDependencyNode& _makeNurbNode;
};

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

void CompareDataSource(HdDataSourceBaseHandle dataSource, std::filesystem::path referencePath)
{
    std::stringstream dataSourceDump;
    HdDebugPrintDataSource(dataSourceDump, dataSource);

    /*std::ofstream outFile(referencePath.filename());
    HdDebugPrintDataSource(outFile, dataSource);*/

    std::stringstream referenceDump;
    std::ifstream referenceFile(referencePath);
    referenceDump << referenceFile.rdbuf();
    
    EXPECT_EQ(dataSourceDump.str(), referenceDump.str())
        << "Data source dump does not match reference.";
}

void DumpDataSourceToFile(HdDataSourceBaseHandle dataSource, std::filesystem::path dumpPath)
{
    std::ofstream dumpFile(dumpPath);
    HdDebugPrintDataSource(dumpFile, dataSource);
}
} // namespace

TEST(NurbsPrimitives, nurbsSphere)
{
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    PrimEntriesVector foundPrims = inspector.FindPrims(getNurbPrimPredicate("nurbsSphereShape1", HdPrimTypeTokens->mesh));
    ASSERT_EQ(foundPrims.size(), 1u);
    HdSceneIndexPrim meshPrim = foundPrims.front().prim;
    EXPECT_TRUE(meshPrim.primType != TfToken());
    EXPECT_TRUE(meshPrim.dataSource != nullptr);


    HdDataSourceBaseHandle topologyDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, topologyLocator);
    HdDataSourceBaseHandle pointsDataSource
        = HdContainerDataSource::Get(meshPrim.dataSource, pointsLocator);

    std::filesystem::path topologyPath = "D:/dev/maya-hydra-oss/maya-hydra/test/testSamples/"
                                         "testNurbsPrimitives/topologyDataSource.txt";
    std::filesystem::path pointsPath = "D:/dev/maya-hydra-oss/maya-hydra/test/testSamples/"
                                       "testNurbsPrimitives/pointsDataSource.txt";

    CompareDataSource(topologyDataSource, topologyPath);
    CompareDataSource(pointsDataSource, pointsPath);

    /*CompareDataSource(
        meshPrim.dataSource,
        "D:/dev/maya-hydra-oss/maya-hydra/test/testSamples/testNurbsPrimitives/"
        "sphere_topology_new.txt");*/

    /*MDagPath sphereDagPath;
    ASSERT_TRUE(GetDagPathFromNodeName("nurbsSphere1", sphereDagPath));
    sphereDagPath.extendToShape();

    MStatus         nurbsResult;
    MFnNurbsSurface nurbsSphere(sphereDagPath.node(), &nurbsResult);*/

    MObject makeNurbNode;
    ASSERT_TRUE(GetDependNodeFromNodeName("makeNurbSphere1", makeNurbNode));
    MStatus makeNurbNodeFnStatus;
    MFnDependencyNode makeNurbNodeFn(makeNurbNode, &makeNurbNodeFnStatus);
    ASSERT_TRUE(makeNurbNodeFnStatus);
    NurbMaker nurbMaker(makeNurbNodeFn);
    
    EXPECT_TRUE(nurbMaker.setAttribute("radius", 10));

    M3dView::active3dView().refresh();

    //CompareDataSource(topologyDataSource, topologyPath);
    //CompareDataSource(pointsDataSource, pointsPath);
}
