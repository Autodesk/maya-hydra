#
# Copyright 2024 Autodesk, Inc. All rights reserved.
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

import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import mayaUtils
import ufe
import platform

class TestDataProducerSelectionHighlighting(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    pSphere1UfeItem = None
    pSphere2UfeItem = None
    pTorusUfeItem = None
    pPlaneUfeItem = None
    shapeNode = "sample_usdShape"
        
    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 0.2

    def loadFileAndInitUfeVariables(self):
        import usdUtils # usdUtils imports mayaUsd.ufe
        from mayaUsd import lib as mayaUsdLib

        testFile = mayaUtils.openTestScene(
                "testWireframeSelectionHighlight",
                "testSelectionHighlightMayaUsd.ma")

        shapeStage = mayaUsdLib.GetPrim(self.shapeNode).GetStage()
        
        #define ufe items
        sphere1Prim = shapeStage.GetPrimAtPath('/sample_usdShape/pSphere1')
        self.assertIsNotNone(sphere1Prim)
        sphere2Prim = shapeStage.GetPrimAtPath('/sample_usdShape/pSphere2')
        self.assertIsNotNone(sphere2Prim)
        torusPrim = shapeStage.GetPrimAtPath('/sample_usdShape/pTorus1')
        self.assertIsNotNone(torusPrim)
        planePrim = shapeStage.GetPrimAtPath('/sample_usdShape/pPlane1')
        self.assertIsNotNone(planePrim)

        ufeSegment = mayaUtils.createUfePathSegment("|sample_usd|sample_usdShape")

        #Sphere1
        pSphere1UfePath = ufe.Path([ufeSegment, usdUtils.createUfePathSegment("/pSphere1")])
        self.pSphere1UfeItem = ufe.Hierarchy.createItem(pSphere1UfePath)
        self.assertIsNotNone(self.pSphere1UfeItem)

        #Sphere2
        pSphere2UfePath = ufe.Path([ufeSegment, usdUtils.createUfePathSegment("/pSphere2")])
        self.pSphere2UfeItem = ufe.Hierarchy.createItem(pSphere2UfePath)
        self.assertIsNotNone(self.pSphere2UfeItem)

        #Torus
        pTorusUfePath = ufe.Path([ufeSegment, usdUtils.createUfePathSegment("/pTorus1")])
        self.pTorusUfeItem = ufe.Hierarchy.createItem(pTorusUfePath)
        self.assertIsNotNone(self.pTorusUfeItem)

        #Plane
        pPlaneUfePath = ufe.Path([ufeSegment, usdUtils.createUfePathSegment("/pPlane1")])
        self.pPlaneUfeItem = ufe.Hierarchy.createItem(pPlaneUfePath)
        self.assertIsNotNone(self.pPlaneUfeItem)
        

    def test_UsdKeepSelectionHighlighting(self):
        
        self.loadFileAndInitUfeVariables()

        #Set VP2 as the current renderer
        self.setViewport2Renderer()
        #Remove anti-aliasing
        cmds.setAttr("hardwareRenderingGlobals.multiSampleEnable", False)
        cmds.refresh()

        #Select the Usd prims to see the selection highlight and their colors respecting the lead object
        ufeGlobalSel =  ufe.GlobalSelection.get()
        ufeGlobalSel.clear()
        ufeGlobalSel.append(self.pSphere1UfeItem)
        ufeGlobalSel.append(self.pSphere2UfeItem)
        ufeGlobalSel.append(self.pTorusUfeItem)
        ufeGlobalSel.append(self.pPlaneUfeItem)
        self.assertSnapshotClose("VP2_AllSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Switch to Storm, we should keep the selected items and their color
        self.setHdStormRenderer()
        self.assertSnapshotClose("Storm_AllSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Switch to wireframe display mode, we should keep the selected items and their color
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayAppearance="wireframe")
        self.assertSnapshotClose("Storm_Wireframe_AllSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Switch to bounding box display mode, we should keep the selected items and their color
        cmds.modelEditor(panel, edit=True, displayAppearance="boundingBox")
        self.assertSnapshotClose("Storm_BoundingBox_AllSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Switch to wireframe on shaded display mode, we should keep the selected items and their color
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=True)
        self.assertSnapshotClose("Storm_WireOnShaded_AllSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_LeadAndActiveColorsSelectionHighlighting(self):
        
        self.loadFileAndInitUfeVariables()

        #Switch to HdStorm
        self.setHdStormRenderer()
        cmds.refresh()

        #select objects to check if the lead / active selection highlight colors work
        ufeGlobalSel =  ufe.GlobalSelection.get()
        ufeGlobalSel.clear()
        ufeGlobalSel.append(self.pSphere1UfeItem)
        self.assertSnapshotClose("Storm_S1Selected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.append(self.pSphere2UfeItem)
        self.assertSnapshotClose("Storm_S1And2Selected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.append(self.pTorusUfeItem)
        self.assertSnapshotClose("Storm_S1And2AndTSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        ufeGlobalSel.append(self.pPlaneUfeItem)
        self.assertSnapshotClose("Storm_S1And2AndTAndPSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Switch to wireframe display mode, we should keep the selected items and their color
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayAppearance="wireframe")
        self.assertSnapshotClose("Storm_Wire_AllSelInStorm.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Remove the lead object
        ufeGlobalSel.remove(self.pPlaneUfeItem)
        self.assertSnapshotClose("Storm_S1And2AndTSelectedPRemoved.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Remove a non lead object
        ufeGlobalSel.remove(self.pSphere1UfeItem)
        self.assertSnapshotClose("Storm_S2AndTSelPAndS1Removed.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
    
    #We want to check that when we select the maya usd proxy shape node it highlights all primitives from the stage
    def test_MayaUsdNodesSelectionHighlighting(self):
        
        self.loadFileAndInitUfeVariables()

        #Switch to HdStorm
        self.setHdStormRenderer()
        cmds.refresh()
    
        cmds.select(self.shapeNode)
        self.assertSnapshotClose("selectMayaUsdNode.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
