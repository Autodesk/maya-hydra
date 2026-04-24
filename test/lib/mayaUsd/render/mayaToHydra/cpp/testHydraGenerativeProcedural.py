# Copyright 2026 Autodesk
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
import ufe
import unittest

import testUtils
from testUtils import PluginLoaded

class TestHydraGenerativeProcedural(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1 

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testHydraGenerativeProcedural', 'simpleHydraGenerativeProcedural.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestHydraGenerativeProcedural, self).setUp()
        self.loadUsdScene()
        self.setBasicCam(10)
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.refresh()

    def test_MaterialBinding(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="HydraGenerativeProcedural.testMaterialBinding")

    def test_SelectionWireframeHighlight(self):
        if self._usdVersion < (0, 25, 11):
            self.skipTest(
                "Selection highlight for generative procedural requires HdGp resolving SI "
                "instantiation earlier; skipped for USD < 25.11 (DLL export / link limitation)."
            )

        sn = ufe.GlobalSelection.get()
        sn.clear()

        stagePathSegment = "|simpleHydraGenerativeProcedural|simpleHydraGenerativeProceduralShape"
        proceduralPath = stagePathSegment + "," + "/MyGenerativeProcedural"
        proceduralItem = ufe.Hierarchy.createItem(ufe.PathString.path(proceduralPath))
        self.assertIsNotNone(proceduralItem)

        sn.append(proceduralItem)
        self.assertSnapshotClose("selHighlight_procedural.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_PickGeneratedChild(self):
        if self._usdVersion < (0, 25, 11):
            self.skipTest(
                "GP picking requires HdGp resolving SI instantiation "
                "earlier; skipped for USD < 25.11 (DLL export / link limitation)."
            )

        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                "cube0", "mesh",                   
                "MyGenerativeProcedural",
                f="HydraGenerativeProcedural.testPickGeneratedChild")

    def test_Transforms(self):
        import mayaUsd
        from pxr import UsdGeom, Gf

        if self._usdVersion < (0, 25, 11):
            self.skipTest(
                "Transforms for generative procedural requires HdGp resolving SI "
                "instantiation earlier; skipped for USD < 25.11 (DLL export / link limitation)."
            )

        stagePathSegment = "|simpleHydraGenerativeProcedural|simpleHydraGenerativeProceduralShape"
        stage = mayaUsd.lib.GetPrim(stagePathSegment).GetStage()
        proceduralPrim = stage.GetPrimAtPath("/MyGenerativeProcedural")
        
        UsdGeom.XformCommonAPI(proceduralPrim).SetTranslate((2, 1, 2))
        cmds.refresh()
        self.assertSnapshotClose("translate_procedural.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        UsdGeom.XformCommonAPI(proceduralPrim).SetScale((2, 1, 2))
        cmds.refresh()
        self.assertSnapshotClose("scale_procedural.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        UsdGeom.XformCommonAPI(proceduralPrim).SetRotate(Gf.Vec3f(0, 45, 0))
        cmds.refresh()
        self.assertSnapshotClose("rotate_procedural.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_Visibility(self):
        import mayaUsd
        from pxr import UsdGeom

        if self._usdVersion < (0, 25, 11):
            self.skipTest(
                "Visibility for generative procedural requires HdGp resolving SI "
                "instantiation earlier; skipped for USD < 25.11 (DLL export / link limitation)."
            )

        stagePathSegment = "|simpleHydraGenerativeProcedural|simpleHydraGenerativeProceduralShape"
        stage = mayaUsd.lib.GetPrim(stagePathSegment).GetStage()
        proceduralPrim = stage.GetPrimAtPath("/MyGenerativeProcedural")
        imageable = UsdGeom.Imageable(proceduralPrim)
        self.assertIsNotNone(imageable)

        # Initially visible
        cmds.refresh()
        self.assertSnapshotClose("visibility_visible.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Make invisible
        imageable.MakeInvisible()
        cmds.refresh()
        self.assertSnapshotClose("visibility_invisible.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Make visible again
        imageable.MakeVisible()
        cmds.refresh()
        self.assertSnapshotClose("visibility_visible.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
