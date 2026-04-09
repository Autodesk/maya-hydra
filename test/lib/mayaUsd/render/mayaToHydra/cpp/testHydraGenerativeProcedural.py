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
import platform
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
        if platform.system() == "Windows" and self._usdVersion < (0, 25, 11):
            self.skipTest(
                "Selection highlight for generative procedural requires HdGp resolving SI "
                "instantiation; skipped on Windows for USD < 25.11 (DLL export / link limitation)."
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
        

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
