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
#include <flowViewport/API/fvpFilteringSceneIndexInterface.h>
#include <flowViewport/API/samples/fvpFilteringSceneIndexClientExample.h>

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

TEST(FlowViewportAPI, filterPrimitives)
{
    /* The python script testFlowViewportAPIFilterPrims.py associated with that file has created 1 cube and 2 spheres with :
    * #Create a maya sphere named parentSphere
        cmds.polyCube(name="parentCube", w=2, h=2, d=2) 
        cmds.polySphere(name="smallSphere") 
        cmds.polySphere(name="bigSphere", subdivisionsAxis=200, subdivisionsHeight= 200) 
    */
    static const MString parentName("parentCube");
    static const MString parentShapeName("parentCubeShape");
    static const MString smallSphereShapeName("smallSphereShape");
    static const MString bigSphereShapeName("bigSphereShape");
    
    //Get the maya nodes which have been created by the python script matching this cpp file.
    const MString names[] = {parentName, parentShapeName, smallSphereShapeName, bigSphereShapeName};
    const MStringArray nameArs (names, 4);
    MObjectArray objArray;
    objArray.setLength(4);
    MStatus stat = MayaHydra::GetObjectsFromNodeNames(nameArs, objArray);
    ASSERT_EQ(stat, MS::kSuccess);

    MObject parentMOject = objArray[0];
    ASSERT_FALSE(parentMOject.isNull());
    MObject parentShapeMOject = objArray[1];
    ASSERT_FALSE(parentShapeMOject.isNull());
    MObject smallSphereShapeMOject = objArray[2];
    ASSERT_FALSE(smallSphereShapeMOject.isNull());
    MObject bigSphereShapeMOject = objArray[3];
    ASSERT_FALSE(bigSphereShapeMOject.isNull());
    
    //FilteringSceneIndexClientExample is an helper class to apply a filtering scene index into the viewport which hides objects with more than 10 000 vertices
    //This is the case for the object named "bigSphere", it has more than 10 000 vertices.
    Fvp::FilteringSceneIndexClientExample hydraViewportFilteringSceneIndexExample ("TestFilteringSceneIndex", 
                                                        Fvp::FilteringSceneIndexClient::Bucket::kSceneFiltering,
                                                        FvpViewportAPITokens->allRenderers,
                                                        &parentShapeMOject //Set the cube as the parent of this filtering scene index
                                                        );
    //Filtering scene index interface
    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    
    //Register it
    const bool bRes = filteringSceneIndexInterface.registerFilteringSceneIndexClient(hydraViewportFilteringSceneIndexExample);
    ASSERT_TRUE(bRes);

    //Check that there are primitives in the viewport terminal scene index
    const SceneIndicesVector& sceneIndices = GetTerminalSceneIndices();
    ASSERT_GT(sceneIndices.size(), static_cast<size_t>(0));
    SceneIndexInspector inspector(sceneIndices.front());

    // Retrieve the small sphere prim in the list of primitives and return its visibility
    FindPrimPredicate findSmallSpherePrimPredicate
        = [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        const std::string primPathString    = primPath.GetAsString();
        HdSceneIndexPrim prim               = sceneIndex->GetPrim(primPath);
        if (primPathString.find(smallSphereShapeName.asChar()) != std::string::npos) {
            //Check if it is visible or not
            auto visibilityHandle = HdVisibilitySchema::GetFromParent(prim.dataSource).GetVisibility();
            if (visibilityHandle){
                return visibilityHandle->GetTypedValue(0.0f); //return true if it is visible, false otherwise
            }
        }
        return false;
    };

    // Retrieve the big sphere prim in the list of primitives and return its visibility
    FindPrimPredicate findBigSpherePrimPredicate
        = [](const HdSceneIndexBasePtr& sceneIndex, const SdfPath& primPath) -> bool {
        const std::string primPathString    = primPath.GetAsString();
        HdSceneIndexPrim prim               = sceneIndex->GetPrim(primPath);
        if (primPathString.find(bigSphereShapeName.asChar()) != std::string::npos) {
            //Check if it is visible or not
            auto visibilityHandle = HdVisibilitySchema::GetFromParent(prim.dataSource).GetVisibility();
            if (visibilityHandle){
                return visibilityHandle->GetTypedValue(0.0f); //return true if it is visible, false otherwise
            }
        }
        return false;
    };

    PrimEntriesVector foundPrims = inspector.FindPrims(findSmallSpherePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), static_cast<size_t>(1)); //The small sphere should be found and visible

    foundPrims = inspector.FindPrims(findBigSpherePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), static_cast<size_t>(0)); //The big sphere should be filtered (not visible)

    //Hide the cube shape node which is the parent node of the filtering scene index, this should disable the filtering and make the big sphere visible.
    MFnDependencyNode depNode(parentShapeMOject, &stat);
    ASSERT_EQ(stat, MS::kSuccess);
    MPlug visibilityPlug = depNode.findPlug("visibility");
    ASSERT_FALSE(visibilityPlug.isNull());
    visibilityPlug.setBool(false);
    
    foundPrims = inspector.FindPrims(findBigSpherePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), static_cast<size_t>(1));//The big sphere should be visible, as the filtering is disabled since the cube which is its parent node is hidden.

    //Unhide the cube shape node
    visibilityPlug.setBool(true);
    
    foundPrims = inspector.FindPrims(findBigSpherePrimPredicate, 1);
    ASSERT_EQ(foundPrims.size(), static_cast<size_t>(0));//The big sphere should not be visible, as filtering is applied again

    filteringSceneIndexInterface.unregisterFilteringSceneIndexClient(hydraViewportFilteringSceneIndexExample);
}
