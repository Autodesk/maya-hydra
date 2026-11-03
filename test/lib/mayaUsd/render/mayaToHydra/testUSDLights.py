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
import mayaUsd.lib
import platform
import unittest

STAGE_SHAPE_PATH = '|testUSDLights:stage1|testUSDLights:stageShape1'
DISK_LIGHT_PRIM_PATH = '/DiskLight1'
DISK_LIGHT_PATH = STAGE_SHAPE_PATH + ',' + DISK_LIGHT_PRIM_PATH
DISTANT_LIGHT_PATH = STAGE_SHAPE_PATH + ',/DistantLight1'

class TestUSDLights(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def verifyLightingModes(self, shadowOn):
        # allLights / domeLight / flatLight changed in USD 25.11.
        imageVersion = "usd25.11+" if self._usdVersion >= (0, 25, 11) else None

        # The USD sphere picks up slightly different dome lighting in USD 26.05+.
        domeImageVersion = ("usd26.05+" if self._usdVersion >= (0, 26, 5)
                            else imageVersion)

        imageSuffix = "_shadowOn" if shadowOn else ""
        panel = mayaUtils.activeModelPanel()

        #Turn on/off shadows
        cmds.modelEditor(panel, edit=True, shadows=shadowOn)

        #All Lights mode
        cmds.modelEditor(panel, edit=True, displayLights="all")
        cmds.refresh()
        self.assertSnapshotClose("allLights" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, imageVersion)

        #Default Light mode
        cmds.modelEditor(panel, edit=True, displayLights="default")
        cmds.refresh()
        self.assertSnapshotClose("defaultLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Selected Light mode
        cmds.modelEditor(panel, edit=True, displayLights="selected")
        cmds.select( clear=True )

        cmds.select( '|testUSDLights:stage1|testUSDLights:stageShape1,/DistantLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("distantLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.select( '|testUSDLights:stage1|testUSDLights:stageShape1,/DiskLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("diskLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.select( '|testUSDLights:stage1|testUSDLights:stageShape1,/DomeLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("domeLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, domeImageVersion)

        #Flat Light mode
        cmds.modelEditor(panel, edit=True, displayLights="flat")
        cmds.refresh()
        self.assertSnapshotClose("flatLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, imageVersion)

        #No Light mode
        cmds.modelEditor(panel, edit=True, displayLights="none")
        cmds.refresh()
        self.assertSnapshotClose("noLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test usd lights (e.g., disk,distant,dome,etc.) with a maya native sphere and usd sphere.
    def test_USDLights(self):
        # Load a maya scene with a maya native sphere, usd sphere and some lights.
        # The sphere is not at the origin on purpose
        testFile = mayaUtils.openTestScene(
                "testUSDLights",
                "testUSDLights.ma")
        self.setTestRenderer()
        cmds.refresh()

        #Test Lighting Modes
        #Shadow OFF
        self.verifyLightingModes(False)
        #TODO: Enable code below when shadows are supported by usd lights
        #Shadow ON
        #self.verifyLightingModes(True)

    def _openSceneInSelectedLightsMode(self):
        mayaUtils.openTestScene("testUSDLights", "testUSDLights.ma")
        self.setTestRenderer()
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, shadows=False)
        cmds.modelEditor(panel, edit=True, displayLights="selected")
        cmds.refresh()

    # Selects the distant and disk lights, the disk light last so that it is the lead object, then
    # deselects the distant light. That changes no lead, so only the selection change of the
    # distant light can turn it off.
    def _verifyDeselectNonLeadLight(self):
        cmds.select(DISTANT_LIGHT_PATH, r=True)
        cmds.select(DISK_LIGHT_PATH, add=True)
        cmds.refresh()

        cmds.select(DISTANT_LIGHT_PATH, deselect=True)
        cmds.refresh()
        imageVersion = "usd26.05+" if self._usdVersion >= (0, 26, 5) else None
        self.assertSnapshotClose("diskLight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, imageVersion)

    def test_USDLightsDeselectNonLeadLight(self):
        self._openSceneInSelectedLightsMode()
        self._verifyDeselectNonLeadLight()

    # Lights removed and added once Hydra is running must still follow their selection.
    def test_USDLightsReAddedLightFollowsSelection(self):
        self._openSceneInSelectedLightsMode()

        stage = mayaUsd.lib.GetPrim(STAGE_SHAPE_PATH).GetStage()
        diskLight = stage.GetPrimAtPath(DISK_LIGHT_PRIM_PATH)
        self.assertTrue(diskLight.IsValid())
        diskLight.SetActive(False)
        cmds.refresh()
        diskLight.SetActive(True)
        cmds.refresh()

        self._verifyDeselectNonLeadLight()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
