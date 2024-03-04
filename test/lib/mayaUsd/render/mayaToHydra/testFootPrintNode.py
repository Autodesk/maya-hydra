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
import mayaUtils
import maya.mel as mel
from testUtils import PluginLoaded

HD_STORM = "HdStormRendererPlugin"
HD_STORM_OVERRIDE = "mayaHydraRenderOverride_" + HD_STORM

class TestFootPrintNode(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        return 0.01

    @property
    def imageDiffFailPercent(self):
        return 0.1

    def tearDown(self):
        #is called after each test : finish by a File New command to check that it's not crashing when cleaning up everything'
        cmds.file(new=True, force=True)

    def setupScene(self):
        self.setHdStormRenderer()
        cmds.move(7.714, 5.786, 7.714, 'persp')
        cmds.refresh()

    #Test adding primitives
    def test_AddingPrimitives(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            self.setupScene()
        
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex
            footPrintNodeName = cmds.createNode("MhFootPrint")
            
            #Increase its size
            cmds.setAttr(footPrintNodeName + '.size', 5)
            cmds.refresh()
            
            #When the node above is created, its compute method is not called automatically, so work around to trigger a call to compute
            cmds.setAttr(footPrintNodeName + '.dummyInput', 2)#setting this will set dirty the dummyOutput attribute
            cmds.getAttr(footPrintNodeName + '.dummyOutput')#getting this value will trigger a call to compute
            cmds.refresh()
            self.assertSnapshotClose("add_NodeCreated.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Move the transform node, the added prims should move as well
            # Get the transform node of the mayaHydraFootPrintNode
            transformNode = cmds.listRelatives(footPrintNodeName, parent=True)[0]
            self.assertIsNotNone(transformNode)
            cmds.move(2, 0.5, -2, transformNode)
            cmds.refresh()
            self.assertSnapshotClose("add_NodeMoved.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        
            #Hide the transform node, this should hide the mayaHydraFootPrintNode node and the added prims as well.
            cmds.hide(transformNode)
            self.assertSnapshotClose("add_NodeHidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Unhide the transform node, this should unhide the mayaHydraFootPrintNode node and the added prims as well.
            cmds.showHidden(transformNode)
            self.assertSnapshotClose("add_NodeUnhidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Delete the shape node, this should hide the added prims as well
            cmds.delete(footPrintNodeName)
            self.assertSnapshotClose("add_NodeDeleted.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            
            #Undo the delete, the node should be visible again so do the added prims
            cmds.undo()
            self.assertSnapshotClose("add_NodeDeletedUndo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Redo the delete, the added prims should be hidden
            cmds.redo()
            self.assertSnapshotClose("add_NodeDeletedRedo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Undo the delete again, the added prims should be visible
            cmds.undo()
            self.assertSnapshotClose("add_NodeDeletedUndoAgain.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            cmds.move(-0.2, -0.5, 0, transformNode)
            cmds.refresh()
            self.assertSnapshotClose("add_NodeMovedAfterDeletionAndUndo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("add_VP2AndThenBackToStorm.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

    #Test FootPrint grids parameters
    def test_FootPrintAttributes(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            self.setupScene()
        
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex and a Filtering scene index
            footPrintNodeName = cmds.createNode("MhFootPrint")

            #When the node above is created, its compute method is not called automatically, so work around to trigger a call to compute
            cmds.setAttr(footPrintNodeName + '.dummyInput', 2)#setting this will set dirty the dummyOutput attribute
            cmds.getAttr(footPrintNodeName + '.dummyOutput')#getting this value will trigger a call to compute
            self.assertSnapshotClose("footPrint_BeforeModifs.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Modify the attributes
            cmds.setAttr(footPrintNodeName + '.size', 3)
            cmds.setAttr(footPrintNodeName + '.color', 1.0, 1.0, 1.0, type="double3")
            cmds.refresh()
            self.assertSnapshotClose("footPrint_AfterModifs.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("footPrint_VP2AndThenBackToStorm.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

    #Test multiple nodes
    def test_MultipleNodes(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            self.setupScene()
        
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex and a Filtering scene index
            footPrintNodeName1 = cmds.createNode("MhFootPrint", n="nodeShape1")

            #When the node above is created, its compute method is not called automatically, so work around to trigger a call to compute
            cmds.setAttr(footPrintNodeName1 + '.dummyInput', 3)#setting this will set dirty the dummyOutput attribute
            cmds.getAttr(footPrintNodeName1 + '.dummyOutput')#getting this value will trigger a call to compute
            
            #Modify the attributes
            cmds.setAttr(footPrintNodeName1 + '.size', 3)
            cmds.setAttr(footPrintNodeName1 + '.color', 1.0, 1.0, 1.0, type="double3")
            cmds.refresh()
            
            #Move the transform node, the added prims should move as well
            # Get the transform node of the mayaHydraFootPrintNode
            transformNode1 = cmds.listRelatives(footPrintNodeName1, parent=True)[0]
            self.assertIsNotNone(transformNode1)
            cmds.move(-2, 0, 0, transformNode1)
            cmds.refresh()
            
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex and a Filtering scene index
            footPrintNodeName2 = cmds.createNode("MhFootPrint", n="nodeShape2")

            #When the node above is created, its compute method is not called automatically, so work around to trigger a call to compute
            cmds.setAttr(footPrintNodeName2 + '.dummyInput', 3)#setting this will set dirty the dummyOutput attribute
            cmds.getAttr(footPrintNodeName2 + '.dummyOutput')#getting this value will trigger a call to compute
            
            cmds.setAttr(footPrintNodeName2 + '.size', 1.5)
            cmds.setAttr(footPrintNodeName2 + '.color', 0.0, 1.0, 0.0, type="double3")
            cmds.refresh()
            
            #Move the transform node, the added prims should move as well
            # Get the transform node of the mayaHydraFootPrintNode
            transformNode2 = cmds.listRelatives(footPrintNodeName2, parent=True)[0]
            self.assertIsNotNone(transformNode2)
            cmds.move(2, 0, -2, transformNode2)
            cmds.refresh()
            
            self.assertSnapshotClose("multipleNodes_BeforeModifs.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Modify the color of node #2, it shouldn't change node's #1 color
            cmds.setAttr(footPrintNodeName2 + '.color', 1.0, 0.0, 0.0, type="double3")

            # Apply transform on node #2
            cmds.select(transformNode2)
            cmds.rotate(0, 45, 0)
            cmds.scale(4, 1, 1)
            cmds.refresh()
            self.assertSnapshotClose("multipleNodes_AfterModifs.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Hide node #1
            cmds.hide(transformNode1)
            self.assertSnapshotClose("multipleNodes_Node1Hidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Unhide node #1
            cmds.showHidden(transformNode1)
            self.assertSnapshotClose("multipleNodes_Node1Unhidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Switch to VP2
            self.setViewport2Renderer()
            #Switch back to Storm
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleNodes_VP2AndThenBackToStorm.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

    #Test multiple viewports
    def test_MultipleViewports(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            #switch to 4 views
            mel.eval('FourViewLayout')
            #Set focus on persp view
            cmds.setFocus ('modelPanel4') #Is the persp view
            #Set Storm as the renderer
            self.setHdStormRenderer()
            modelPanel4 = cmds.playblast(activeEditor=1)
            rendererOverrideNameModPanel4 = cmds.modelEditor(modelPanel4, q=1,rendererOverrideName=1)
            self.assertTrue(rendererOverrideNameModPanel4 == HD_STORM_OVERRIDE)

            #Set focus on model Panel 1 (it's an orthographic view : top) 
            cmds.setFocus ('modelPanel1')
            #Set Storm as the renderer
            self.setHdStormRenderer()
            modelPanel1 = cmds.playblast(activeEditor=1)
            rendererOverrideNameModPanel1 = cmds.modelEditor(modelPanel1, q=1,rendererOverrideName=1)
            self.assertTrue(rendererOverrideNameModPanel1 == HD_STORM_OVERRIDE)
            
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex and a Filtering scene index
            footPrintNodeName1 = cmds.createNode("MhFootPrint", n="nodeShape1")

            #When the node above is created, its compute method is not called automatically, so work around to trigger a call to compute
            cmds.setAttr(footPrintNodeName1 + '.dummyInput', 2)#setting this will set dirty the dummyOutput attribute
            cmds.getAttr(footPrintNodeName1 + '.dummyOutput')#getting this value will trigger a call to compute
            
            cmds.setAttr(footPrintNodeName1 + '.size', 6)
            cmds.setAttr(footPrintNodeName1 + '.color', 0.0, 1.0, 0.0, type="double3")
            cmds.refresh()
            
            cmds.setFocus ('modelPanel4')
            self.assertSnapshotClose("multipleViewports_viewPanel4.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            cmds.setFocus ('modelPanel1')
            self.assertSnapshotClose("multipleViewports_viewPanel1.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

            #Switch to VP2
            cmds.setFocus ('modelPanel4')
            self.setViewport2Renderer()
            self.assertSnapshotClose("multipleViewports_VP2_modPan4.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            cmds.setFocus ('modelPanel1')
            self.setViewport2Renderer()
            self.assertSnapshotClose("multipleViewports_VP2_modPan3.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            
            #Switch back to Storm
            cmds.setFocus ('modelPanel4')
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleViewports_VP2AndThenBackToStorm_modPan4.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            cmds.setFocus ('modelPanel1')
            self.setHdStormRenderer()
            self.assertSnapshotClose("multipleViewports_VP2AndThenBackToStorm_modPan3.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

    #Test loading a scene
    def test_Load(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            cmds.file(new=True, force=True)
            testFile = mayaUtils.openTestScene( 
                "testFootPrintNode",
                "testFootPrintNodeSaved.ma")
            cmds.refresh()
            self.assertSnapshotClose("loadingFootPrintScene.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
