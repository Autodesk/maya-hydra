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

import platform

class TestMayaLights(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01

    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        # HYDRA-943 : Shadows don't seem to work properly on OSX, possibly only with point lights.
        # Only the "all lights with shadows" check fails, and shadows with point light only is skipped.
        if platform.system() == "Darwin":
            return 3
        return 0.2

    def verifyLightingModes(self, shadowOn):
        imageSuffix = "_shadowOn" if shadowOn else ""
        panel = mayaUtils.activeModelPanel()

        #Turn on/off shadows
        cmds.modelEditor(panel, edit=True, shadows=shadowOn)

        #All Lights mode
        cmds.modelEditor(panel, edit=True, displayLights="all")
        cmds.refresh()
        self.assertSnapshotClose("allLights" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Default Light mode
        cmds.modelEditor(panel, edit=True, displayLights="default")
        cmds.refresh()
        self.assertSnapshotClose("defaultLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Selected Light mode
        cmds.modelEditor(panel, edit=True, displayLights="selected")
        cmds.select( clear=True )
        #Use Directional Light
        cmds.select( 'directionalLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("directionalLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Use Point Light
        #TODO: Enable shadowOn test on point light when it works
        if not shadowOn:
            cmds.select( 'pointLight1', r=True )
            cmds.refresh()
            self.assertSnapshotClose("pointLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Use Spot Light
        cmds.select( 'spotLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("spotLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Flat Light mode
        #TODO: Enable test on flat lighting mode when it works
        #cmds.modelEditor(panel, edit=True, displayLights="flat")
        #cmds.refresh()
        #self.assertSnapshotClose("flatLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #No Light mode
        if not shadowOn:
            cmds.modelEditor(panel, edit=True, displayLights="none")
            cmds.refresh()
            self.assertSnapshotClose("noLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test maya lights (e.g., default,directional,point,spot,etc.) with a maya native sphere and usd sphere.
    def test_MayaLights(self):
        # Load a maya scene with a maya native sphere, usd sphere and some lights, with HdStorm already being the viewport renderer.
        # The sphere is not at the origin on purpose
        testFile = mayaUtils.openTestScene(
                "testMayaLights",
                "testMayaLights.ma")
        cmds.refresh()

        #Test Lighting Modes
        #Shadow OFF
        self.verifyLightingModes(False)
        #Shadow ON
        self.verifyLightingModes(True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
