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
import unittest

class TesMayatLights(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.3

    def verifyLightingModes(self, shadowOn):
        imageSuffix = "_shadowOn" if shadowOn else ""
        panel = mayaUtils.activeModelPanel()

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

        cmds.select( 'directionalLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("directionalLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.select( 'pointLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("pointLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.select( 'spotLight1', r=True )
        cmds.refresh()
        self.assertSnapshotClose("spotLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Flat Light mode
        cmds.modelEditor(panel, edit=True, displayLights="flat")
        cmds.refresh()
        self.assertSnapshotClose("flatLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #No Light mode
        cmds.modelEditor(panel, edit=True, displayLights="none")
        cmds.refresh()
        self.assertSnapshotClose("noLight" + imageSuffix + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    #Test maya lights (e.g., default,directional,point,spot,etc.) with a maya native sphere and usd sphere.
    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_MayaLights(self):
        cmds.file(new=True, force=True)
        cmds.refresh()

        # Load a maya scene with a maya native sphere, usd sphere and some lights, with HdStorm already being the viewport renderer.
        # The sphere is not at the origin on purpose
        testFile = mayaUtils.openTestScene(
                "testMayaLights",
                "testMayaLights.ma")
        cmds.refresh()
        #Do a view fit --Test removed as on Linux the fit doesn't produce the same result as on Windows and OSX.
        #cmds.viewFit('persp')
        #cmds.refresh()
        #self.assertSnapshotClose("allLightsFit.png", None, None)

        #Test Lighting Modes
        #Shadow OFF
        self.verifyLightingModes(False)
        #Shadow ON
        self.verifyLightingModes(True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())