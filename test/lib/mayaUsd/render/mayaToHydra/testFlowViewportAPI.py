#
# Copyright 2023 Autodesk, Inc. All rights reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#

from math import*
import maya.cmds as cmds
import maya.api.OpenMaya as om
import fixturesUtils
import mtohUtils
import maya.mel as mel
from testUtils import PluginLoaded

def setRotateY(matrixAsAList, angle):
    ''' Sets the matrix as a list of values to be a Rotate about Y matrix (deg), and returns it'''
    angle *=  (pi/180);
    matrixAsAList[0] = cos(angle)
    matrixAsAList[2+4*2] = cos(angle)
    matrixAsAList[0+4*2] = -sin(angle)
    matrixAsAList[2+4*0] = sin(angle)
    return matrixAsAList

class TestFlowViewportAPI(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    @classmethod
    def tearDownClass(cls):
        #Finish by a File New command to check that it's not crashing when cleaning up everything'
        cmds.file(new=True, force=True)

    def setupScene(self):
        self.setHdStormRenderer()

    def tearDown(self):
        #is called after each test : finish by a File New command to check that it's not crashing when cleaning up everything'
        cmds.file(new=True, force=True)

    #Test adding primitives
    def test_AddingPrimitives(self):
        self.setupScene()
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            
            #Create a maya sphere
            sphereNode, sphereShape = cmds.polySphere()
            cmds.refresh()

            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName = cmds.createNode("MhFlowViewportAPILocator")

            #Original images are located for example in maya-hydra\test\lib\mayaUsd\render\mayaToHydra\FlowViewportAPITest
            self.assertSnapshotClose("add_NodeCreated.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Move the transform node, the added prims (cube grid) should move as well
            # Get the transform node of the MhFlowViewportAPILocator
            transformNode = cmds.listRelatives(flowViewportNodeName, parent=True)[0]
            self.assertIsNotNone(transformNode)
            cmds.move(10, 5, -5, transformNode)
            cmds.refresh()
            self.assertSnapshotClose("add_NodeMoved.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
            #Hide the transform node, this should hide the MhFlowViewportAPILocator node and the added prims as well.
            cmds.hide(transformNode)
            self.assertSnapshotClose("add_NodeHidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Unhide the transform node, this should unhide the MhFlowViewportAPILocator node and the added prims as well.
            cmds.showHidden(transformNode)
            self.assertSnapshotClose("add_NodeUnhidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Delete the shape node, this should hide the added prims as well
            cmds.delete(flowViewportNodeName)
            self.assertSnapshotClose("add_NodeDeleted.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Undo the delete, the node should be visible again so do the added prims
            cmds.undo()
            self.assertSnapshotClose("add_NodeDeletedUndo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Redo the delete, the added prims should be hidden
            cmds.redo()
            self.assertSnapshotClose("add_NodeDeletedRedo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Undo the delete again, the added prims should be visible
            cmds.undo()
            self.assertSnapshotClose("add_NodeDeletedUndoAgain.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Move transform node again to see if it still updates the added prims transform
            cmds.move(-20, -5, 0, transformNode)
            cmds.refresh()
            self.assertSnapshotClose("add_NodeMovedAfterDeletionAndUndo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("add_VP2AndThenBackToStorm.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test filtering primitives
    def test_FilteringPrimitives(self):
        self.setupScene()
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):

            #Create a maya sphere
            sphereNode, sphereShape = cmds.polySphere()
            cmds.refresh()

            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName = cmds.createNode("MhFlowViewportAPILocator")
            cmds.refresh()
            #Original images are located for example in maya-hydra\test\lib\mayaUsd\render\mayaToHydra\FlowViewportAPITest
            self.assertSnapshotClose("filter_NodeCreated.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Move the transform node, the added prims (cube grid) should move as well
            # Get the transform node of the MhFlowViewportAPILocator
            transformNode = cmds.listRelatives(flowViewportNodeName, parent=True)[0]
            self.assertIsNotNone(transformNode)
            cmds.move(15, 0, 0, transformNode)
            cmds.refresh()
            self.assertSnapshotClose("filter_NodeMoved.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Change sphere attributes to add more vertices/polygons, our filtering hides a prim when its number of vertices is greater than 10 000.
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 200)
            cmds.setAttr(sphereShape + '.subdivisionsHeight', 200)
            cmds.refresh()
            self.assertSnapshotClose("filter_SphereFiltered.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Decreasing the number of vertices of this sphere under 10 000 should make it visible again (not filtered)
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 30)
            cmds.refresh()
            self.assertSnapshotClose("filter_SphereUnFiltered.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Increasing again the number of vertices above 10 000 should make it filtered again (invisible)
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 200)
            cmds.refresh()
            self.assertSnapshotClose("filter_SphereFilteredAgain.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Hide the transform node, this should hide the MhFlowViewportAPILocator shape node and disable the filtering as well.
            cmds.hide(transformNode)
            self.assertSnapshotClose("filter_NodeHidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Unhide the transform node, this should unhide the MhFlowViewportAPILocator node and enable the filtering as well.
            cmds.showHidden(transformNode)
            self.assertSnapshotClose("filter_NodeUnhidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Delete the shape node, this should disable filtering
            cmds.delete(flowViewportNodeName)
            self.assertSnapshotClose("filter_NodeDeleted.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Undo the delete, the node should be visible again and filtering be enabled
            cmds.undo()
            self.assertSnapshotClose("filter_NodeDeletedUndo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Redo the delete, filtering should be disabled
            cmds.redo()
            self.assertSnapshotClose("filter_NodeDeletedRedo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Undo the delete so filtering is enabled again
            cmds.undo()

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("filter_VP2AndThenBackToStorm.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Test unfiltering the sphere after switching renderers (HYDRA-747)
            self.setViewport2Renderer()
            cmds.xform(sphereNode, translation=[0,5,0], scale=[4,4,4])
            self.setHdStormRenderer()
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 30) #Unfilter the prim
            cmds.refresh()
            self.assertSnapshotClose("filter_VP2AndThenBackToStorm_MovedSphereUnFiltered.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    #Test Cube grids parameters
    def test_CubeGrid(self):
        self.setupScene()
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            
            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName = cmds.createNode("MhFlowViewportAPILocator")
            self.assertSnapshotClose("cubeGrid_BeforeModifs.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Get the matrix and set a rotation of 70 degress around Y axis.
            matrix = cmds.getAttr(flowViewportNodeName + '.cubeInitalTransform')
            #Set it to have its rotation be a rotation around Y of 70 deg.
            setRotateY(matrix, 70)

            #Modify the cube grid parameters
            cmds.setAttr(flowViewportNodeName + '.numCubesX', 3)
            cmds.setAttr(flowViewportNodeName + '.numCubesY', 2)
            cmds.setAttr(flowViewportNodeName + '.numCubesZ', 3)
            cmds.setAttr(flowViewportNodeName + '.cubeHalfSize', 0.5)
            cmds.setAttr(flowViewportNodeName + '.cubeInitalTransform', matrix, type="matrix")
            cmds.setAttr(flowViewportNodeName + '.cubeColor', 1.0, 1.0, 0.0, type="double3")
            cmds.setAttr(flowViewportNodeName + '.cubeOpacity', 0.2)
            cmds.setAttr(flowViewportNodeName + '.cubesUseInstancing', False)
            cmds.setAttr(flowViewportNodeName + '.cubesDeltaTrans', 15, 15, 15, type="double3")
            cmds.refresh()
            self.assertSnapshotClose("cubeGrid_AfterModifs.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Test instancing
            cmds.setAttr(flowViewportNodeName + '.cubesUseInstancing', True)
            cmds.refresh()
            self.assertSnapshotClose("cubeGrid_WithInstancing.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Add more cubes
            cmds.setAttr(flowViewportNodeName + '.numCubesX', 30)
            cmds.setAttr(flowViewportNodeName + '.numCubesY', 30)
            cmds.setAttr(flowViewportNodeName + '.numCubesZ', 30)
            cmds.setAttr(flowViewportNodeName + '.cubeColor', 0.0, 0.5, 1.0, type="double3")
            cmds.setAttr(flowViewportNodeName + '.cubeOpacity', 0.3)
            cmds.setAttr(flowViewportNodeName + '.cubesDeltaTrans', 5, 5, 5, type="double3")
            cmds.refresh()
            self.assertSnapshotClose("cubeGrid_WithInstancingModifs.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("cubeGrid_VP2AndThenBackToStorm.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test multiple nodes
    def test_MultipleNodes(self):
        self.setupScene()
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            
            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName1 = cmds.createNode("MhFlowViewportAPILocator", n="nodeShape1")
            
            #Get the matrix and set a rotation of 70 degress around Y axis.
            matrix = cmds.getAttr(flowViewportNodeName1 + '.cubeInitalTransform')
            #Set it to have its rotation be a rotation around Y of 70 deg.
            setRotateY(matrix, 70)

            #Modify the cube grid parameters
            cmds.setAttr(flowViewportNodeName1 + '.numCubesX', 3)
            cmds.setAttr(flowViewportNodeName1 + '.numCubesY', 3)
            cmds.setAttr(flowViewportNodeName1 + '.numCubesZ', 3)
            cmds.setAttr(flowViewportNodeName1 + '.cubeHalfSize', 0.5)
            cmds.setAttr(flowViewportNodeName1 + '.cubeInitalTransform', matrix, type="matrix")
            cmds.setAttr(flowViewportNodeName1 + '.cubeColor', 1.0, 0.0, 0.0, type="double3")
            cmds.setAttr(flowViewportNodeName1 + '.cubeOpacity', 0.2)
            cmds.setAttr(flowViewportNodeName1 + '.cubesUseInstancing', False)
            cmds.setAttr(flowViewportNodeName1 + '.cubesDeltaTrans', 5, 5, 5, type="double3")
            cmds.refresh()
            
            #Move the transform node, the added prims (cube grid) should move as well
            # Get the transform node of the MhFlowViewportAPILocator
            transformNode1 = cmds.listRelatives(flowViewportNodeName1, parent=True)[0]
            self.assertIsNotNone(transformNode1)
            cmds.move(-10, 0, 0, transformNode1)
            cmds.refresh()
            
            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName2 = cmds.createNode("MhFlowViewportAPILocator", n="nodeShape2")
            
            #Get the matrix and set a rotation of 70 degress around Y axis.
            matrix = cmds.getAttr(flowViewportNodeName2 + '.cubeInitalTransform')
            #Set it to have its rotation be a rotation around Y of 70 deg.
            setRotateY(matrix, 20)

            #Modify the cube grid parameters
            cmds.setAttr(flowViewportNodeName2 + '.cubesUseInstancing', True) #Setting instancing to true first make it go faster when changing the number of cubes
            cmds.setAttr(flowViewportNodeName2 + '.numCubesX', 10)
            cmds.setAttr(flowViewportNodeName2 + '.numCubesY', 10)
            cmds.setAttr(flowViewportNodeName2 + '.numCubesZ', 1)
            cmds.setAttr(flowViewportNodeName2 + '.cubeHalfSize', 2)
            cmds.setAttr(flowViewportNodeName2 + '.cubeInitalTransform', matrix, type="matrix")
            cmds.setAttr(flowViewportNodeName2 + '.cubeColor', 0.0, 0.0, 1.0, type="double3")
            cmds.setAttr(flowViewportNodeName2 + '.cubeOpacity', 0.8)
            cmds.setAttr(flowViewportNodeName2 + '.cubesDeltaTrans', 10, 10, 10, type="double3")
            cmds.refresh()
            
            #Move the transform node, the added prims (cube grid) should move as well
            # Get the transform node of the MhFlowViewportAPILocator
            transformNode2 = cmds.listRelatives(flowViewportNodeName2, parent=True)[0]
            self.assertIsNotNone(transformNode2)
            cmds.move(-30, 0, -30, transformNode2)
            cmds.refresh()
            
            self.assertSnapshotClose("multipleNodes_BeforeModifs.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Modify the color of node #2, it shouldn't change node's #1 color
            cmds.setAttr(flowViewportNodeName2 + '.cubeColor', 1.0, 1.0, 1.0, type="double3")
            cmds.setAttr(flowViewportNodeName2 + '.cubeOpacity', 0.1)

            # Apply transform on node #2
            cmds.move(-30, 0, 0, transformNode2)
            cmds.rotate(-30, 45, 0, transformNode2)
            cmds.scale(2, 1, 1, transformNode2)
            cmds.refresh()
            self.assertSnapshotClose("multipleNodes_AfterModifs.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Remove instancing, the cubes should stay at the same place
            cmds.setAttr(flowViewportNodeName2 + '.cubesUseInstancing', False)
            self.assertSnapshotClose("multipleNodes_AfterModifsRemoveInstancing.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Hide node #1
            cmds.hide(transformNode1)
            self.assertSnapshotClose("multipleNodes_Node1Hidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Unhide node #1
            cmds.showHidden(transformNode1)
            self.assertSnapshotClose("multipleNodes_Node1Unhidden.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleNodes_VP2AndThenBackToStorm.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test multiple viewports
    def test_MultipleViewports(self):
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            #switch to 4 views
            mel.eval('FourViewLayout')
            #Set focus on persp view
            cmds.setFocus ('modelPanel4') #Is the persp view
            #Set Storm as the renderer
            self.setHdStormRenderer()
            
            #Set focus on model Panel 2 (it's an orthographic view : right) 
            cmds.setFocus ('modelPanel2')
            #Set Storm as the renderer
            self.setHdStormRenderer()
            
            #Create a maya sphere
            sphereNode, sphereShape = cmds.polySphere()
            cmds.move(15, 0, 0, sphereNode)
            cmds.refresh()

            #Create a MhFlowViewportAPILocator node which adds a dataProducerSceneIndex and a Filtering scene index
            flowViewportNodeName1 = cmds.createNode("MhFlowViewportAPILocator", n="nodeShape1")
            
            #Modify the cube grid parameters
            cmds.setAttr(flowViewportNodeName1 + '.numCubesX', 3)
            cmds.setAttr(flowViewportNodeName1 + '.numCubesY', 3)
            cmds.setAttr(flowViewportNodeName1 + '.numCubesZ', 3)
            cmds.setAttr(flowViewportNodeName1 + '.cubeHalfSize', 1.0)
            cmds.setAttr(flowViewportNodeName1 + '.cubeColor', 1.0, 0.0, 0.0, type="double3")
            cmds.setAttr(flowViewportNodeName1 + '.cubeOpacity', 0.8)
            cmds.setAttr(flowViewportNodeName1 + '.cubesUseInstancing', False)
            cmds.setAttr(flowViewportNodeName1 + '.cubesDeltaTrans', 3, 3, 3, type="double3")
            cmds.refresh()
            
            cmds.setFocus ('modelPanel4')
            self.assertSnapshotClose("multipleViewports_viewPanel4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.setFocus ('modelPanel2')
            self.assertSnapshotClose("multipleViewports_viewPanel2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Change sphere attributes to add more vertices/polygons, our filtering removes a prim when its number of vertices is greater than 10 000.
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 200)
            cmds.setAttr(sphereShape + '.subdivisionsHeight', 200)
            cmds.refresh()
            cmds.setFocus ('modelPanel4')
            self.assertSnapshotClose("multipleViewports_sphereFiltered_viewPanel4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.setFocus ('modelPanel2')
            self.assertSnapshotClose("multipleViewports_sphereFiltered_viewPanel2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Remove filtering by decreasing the number of vertices
            cmds.setAttr(sphereShape + '.subdivisionsAxis', 30)
            cmds.refresh()
            cmds.setFocus ('modelPanel4')
            self.assertSnapshotClose("multipleViewports_sphereUnfiltered_viewPanel4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.setFocus ('modelPanel2')
            self.assertSnapshotClose("multipleViewports_sphereUnfiltered_viewPanel2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Switch to VP2
            cmds.setFocus ('modelPanel4')
            self.setViewport2Renderer()
            self.assertSnapshotClose("multipleViewports_VP2_modPan4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.setFocus ('modelPanel2')
            self.setViewport2Renderer()
            self.assertSnapshotClose("multipleViewports_VP2_modPan2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            
            #Switch back to Storm
            cmds.setFocus ('modelPanel4')
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleViewports_VP2AndThenBackToStorm_modPan4.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.setFocus ('modelPanel2')
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleViewports_VP2AndThenBackToStorm_modPan2.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
