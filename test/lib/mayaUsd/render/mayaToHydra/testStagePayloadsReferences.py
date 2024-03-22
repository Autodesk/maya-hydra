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
import unittest
import testUtils
import mayaUtils
import ufe
from pxr import Usd, Sdf

class TestUsdStagePayloadsAndReferences(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def loadUsdPayloadScene(self):
        cmds.file(new=True, force=True)
        mayaUtils.openTestScene(
                "testStagePayloadsReferences",
                "FlowerPot.ma")
        self.setHdStormRenderer()
        cmds.refresh()

    def loadUsdReferencesScene(self):
        cmds.file(new=True, force=True)
        mayaUtils.openTestScene(
                "testStagePayloadsReferences",
                "References.ma")
        self.setHdStormRenderer()
        cmds.refresh()

    def setUpReferenceScene(self):
        from mayaUsd import lib as mayaUsdLib
        import mayaUsd_createStageWithNewLayer

        cmds.file(new=True, force=True)

        # Create a simple scene with a Def prim with a USD reference.
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        psPath = ufe.PathString.path(psPathStr)
        ps = ufe.Hierarchy.createItem(psPath)
        stage = mayaUsdLib.GetPrim(psPathStr).GetStage()
        aPrim = stage.DefinePrim('/A', 'Xform')

        sphereFile = testUtils.getTestScene("testStagePayloadsReferences", "cube.usda")
        sdfRef = Sdf.Reference(sphereFile)
        primRefs = aPrim.GetReferences()
        primRefs.AddReference(sdfRef)
        self.assertTrue(aPrim.HasAuthoredReferences())
        self.setHdStormRenderer()

    def setUpPayloadScene(self):
        cmds.file(new=True, force=True)
        import mayaUsd_createStageWithNewLayer
        from mayaUsd import lib as mayaUsdLib
        import usdUtils

        self.setHdStormRenderer()

        # Create the following hierarchy:
        #
        # proxy shape
        #  |_ A
        #  |_ B
        #
 
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        self.stage = mayaUsdLib.GetPrim(psPathStr).GetStage()
        self.stage.DefinePrim('/A', 'Xform')
        self.stage.DefinePrim('/B', 'Xform')
 
        psPath = ufe.PathString.path(psPathStr)
        psPathSegment = psPath.segments[0]
        aPath = ufe.Path([psPathSegment, usdUtils.createUfePathSegment('/A')])
        bPath = ufe.Path([psPathSegment, usdUtils.createUfePathSegment('/B')])
        self.a = ufe.Hierarchy.createItem(aPath)
        self.b = ufe.Hierarchy.createItem(bPath)
 
        cmds.select(clear=True)

    def test_UsdStagePayloadsOnTheFly(self):
        import mayaUsd.ufe
        import usdUfe
    
        self.setUpPayloadScene()
       
        primUfePath = "|stage1|stageShape1,/A"
        prim = mayaUsd.ufe.ufePathToPrim(primUfePath)

        # Verify initial state
        self.assertFalse(prim.HasPayload())
        self.assertSnapshotClose("initialState.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        payloadFile = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')
        cmd = usdUfe.AddPayloadCommand(prim, payloadFile, True)
        self.assertIsNotNone(cmd)

        # Verify state after add payload
        cmd.execute()
        self.assertTrue(prim.HasPayload())
        self.assertTrue(prim.IsLoaded())
        self.assertSnapshotClose("cubeLoaded.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Verify unload payload
        cmd = usdUfe.UnloadPayloadCommand(prim)
        self.assertIsNotNone(cmd)

        cmd.execute()
        self.assertFalse(prim.IsLoaded())
        self.assertSnapshotClose("cubeUnloaded.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmd.undo()
        self.assertTrue(prim.IsLoaded())
        self.assertSnapshotClose("cubeUnloadedUndo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmd.redo()
        self.assertFalse(prim.IsLoaded())
        self.assertSnapshotClose("cubeUnloadedRedo.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Verify load payload
        cmd = usdUfe.LoadPayloadCommand(prim, Usd.LoadWithDescendants)
        self.assertIsNotNone(cmd)

        cmd.execute()
        self.assertTrue(prim.IsLoaded())
        self.assertSnapshotClose("cubeLoadWithDescendants.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_UsdStagePayloadsFromScene(self):
        from mayaUsd import lib as mayaUsdLib
        self.loadUsdPayloadScene()
        self.assertSnapshotClose("payloadSceneLoadedPotA.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
       
        #Change the variant
        shapeNode = "FlowerPotShape"
        shapeStage = mayaUsdLib.GetPrim(shapeNode).GetStage()
        rootPrim = shapeStage.GetPrimAtPath('/FlowerPot')
        self.assertIsNotNone(rootPrim)
        modVariant = rootPrim.GetVariantSet('modelingVariant')
        self.assertIsNotNone(modVariant)
        self.assertEqual(modVariant.GetVariantSelection(), 'FlowerPotA')
        
        modVariant.SetVariantSelection('FlowerPotB')
        self.assertEqual(modVariant.GetVariantSelection(), 'FlowerPotB')
        self.assertSnapshotClose("payloadSceneLoadedPotB.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_UsdStageReferences(self):
        self.setUpReferenceScene()
        self.assertSnapshotClose("referencesSceneCreated.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        self.loadUsdReferencesScene()
        self.assertSnapshotClose("referencesSceneLoaded.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
