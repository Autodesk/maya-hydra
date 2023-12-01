//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

//Local headers
#include "testUtils.h"

//maya hydra
#include <mayaHydraLib/hydraUtils.h>
#include <mayaHydraLib/mayaUtils.h>

//Flow viewport headers
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>
#include <flowViewport/API/samples/fvpDataProducerSceneIndexExample.h>

//maya headers
#include <maya/MFnDependencyNode.h>
#include <maya/MPlug.h>
#include <maya/MObjectArray.h>
#include <maya/MStringArray.h>

//Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>

//Google tests
#include <gtest/gtest.h>

PXR_NAMESPACE_USING_DIRECTIVE

TEST(FlowViewportAPI, addPrimitives)
{
    static const MString sphereName("parentSphere");
    static const MString sphereShapeName("parentSphereShape");
    

    ///3D Grid of cube mesh primitives creation parameters for the data producer scene index
    Fvp::DataProducerSceneIndexExample::CubeGridCreationParams  cubeGridParams;
    cubeGridParams._numLevelsX      = 5;
    cubeGridParams._numLevelsY      = 5;
    cubeGridParams._numLevelsZ      = 1;
    cubeGridParams._color           = GfVec3f(0.0f, 0.0f, 1.0f);
    cubeGridParams._deltaTrans      = GfVec3f(6.0f, 8.0f, 10.0f);
    cubeGridParams._opacity         = 0.8f;
    cubeGridParams._useInstancing   = false;
    cubeGridParams._halfSize        = 3.0f;

    //hydraViewportDataProducerSceneIndexExample is what will inject the 3D grid of Hydra cube mesh primitives into the viewport
    Fvp::DataProducerSceneIndexExample  hydraViewportDataProducerSceneIndexExample;

    const std::string firstCubePath (TfStringPrintf("/DataProducerSceneIndexExample/cube_%p0_0_0", &hydraViewportDataProducerSceneIndexExample));
    
    //Setup cube grid parameters
    hydraViewportDataProducerSceneIndexExample.setCubeGridParams(cubeGridParams);

    //Data producer scene index interface
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    
    //Store the interface pointer into our client for later
    hydraViewportDataProducerSceneIndexExample.setHydraInterface(&dataProducerSceneIndexInterface);
    
    //Set the maya node as a parent, the "parentSphere" maya sphere has been created by the python script matching this cpp file.
    const MString names[] = {sphereName, sphereShapeName};
    const MStringArray nameArs (names, 2);
    MObjectArray objArray;
    objArray.setLength(2);
    MStatus stat = MayaHydra::GetObjectsFromNodeNames(nameArs, objArray);
    ASSERT_EQ(stat, MS::kSuccess);

    MObject parentSphereMOject      = objArray[0];
    ASSERT_FALSE(parentSphereMOject.isNull());
    MObject parentSphereShapeMOject = objArray[1];
    ASSERT_FALSE(parentSphereShapeMOject.isNull());
    ASSERT_TRUE(parentSphereMOject.hasFn(MFn::kTransform));

    hydraViewportDataProducerSceneIndexExample.setContainerNode(&parentSphereShapeMOject);

    //Add the data producer scene index which will create the cube grid in the viewport and the scene indices chain to handle visibility/transform updates and node delete/undelete
    hydraViewportDataProducerSceneIndexExample.addDataProducerSceneIndex();

    //Setup inspector for the first viewport scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), 0u);
    SceneIndexInspector inspector(sceneIndices.front());

    // Retrieve the first cube primitive from its Sdfpath and check its visibility
    const PrimNameVisibilityPredicate findFirstCubePrimPredicate(firstCubePath);
    
    PrimEntriesVector foundPrims = inspector.FindPrims(findFirstCubePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), 1u); //The cube should be found

    //Hide the shape node
    MFnDependencyNode depNode(parentSphereShapeMOject, &stat);
    ASSERT_EQ(stat, MS::kSuccess);
    MPlug visibilityPlug = depNode.findPlug("visibility");
    ASSERT_FALSE(visibilityPlug.isNull());
    visibilityPlug.setBool(false);
    
    foundPrims = inspector.FindPrims(findFirstCubePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), 0u);//The cube should not be found

    //Unhide the shape node
    visibilityPlug.setBool(true);
    
    foundPrims = inspector.FindPrims(findFirstCubePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), 1u);//The cube should be found

    hydraViewportDataProducerSceneIndexExample.removeDataProducerSceneIndex();
}
