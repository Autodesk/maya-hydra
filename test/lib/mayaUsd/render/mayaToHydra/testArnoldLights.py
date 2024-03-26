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
import platform

class TestArnoldLights(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']

    @property
    def imageDiffFailThreshold(self):
        return 0.01

    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
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
        self.assertSnapshotClose("allLights" + imageSuffix + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #TODO: Enable code below when those lighting modes are supported by arnold lights
        #Default Light mode
        #cmds.modelEditor(panel, edit=True, displayLights="default")
        #cmds.refresh()
        #self.assertSnapshotClose("defaultLight" + imageSuffix + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Selected Light mode
        #cmds.modelEditor(panel, edit=True, displayLights="selected")
        #cmds.select( clear=True )

        #cmds.select( 'aiSkyDomeLight1', r=True )
        #cmds.refresh()
        #self.assertSnapshotClose("spotLight" + imageSuffix + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Flat Light mode
        #cmds.modelEditor(panel, edit=True, displayLights="flat")
        #cmds.refresh()
        #self.assertSnapshotClose("flatLight" + imageSuffix + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #No Light mode
        #cmds.modelEditor(panel, edit=True, displayLights="none")
        #cmds.refresh()
        #self.assertSnapshotClose("noLight" + imageSuffix + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

    #Test arnold lights (e.g., aiSkyDomeLight,etc.) with a maya native sphere and usd sphere.
    def test_ArnoldLights(self):
        cmds.file(new=True, force=True)

        # Load a maya scene with a maya native sphere, usd sphere and some lights, with HdStorm already being the viewport renderer.
        # The sphere is not at the origin on purpose
        testFile = mayaUtils.openTestScene(
                "testArnoldLights",
                "testArnoldLights.ma")
        cmds.refresh()

        #Test Lighting Modes
        #Shadow OFF
        self.verifyLightingModes(False)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
