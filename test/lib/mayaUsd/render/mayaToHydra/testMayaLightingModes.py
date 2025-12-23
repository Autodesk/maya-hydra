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
from testUtils import PluginLoaded

import platform

class TestMayaLightingModes(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']
    
    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.5

    def verifySnapshot(self, imageName):
        cmds.refresh()

         # Dome lighting change in USD 25.11+
        imageVersion = None
        if self._usdVersion >= (0, 25, 11):
            imageVersion = "usd2511+"

        self.assertSnapshotClose(imageName, 
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT,
                                 imageVersion)
    
    def test_MayaUsualLightingModes(self):
        # open the Maya scene
        testFile = mayaUtils.openTestScene(
                "testMayaLightingModes",
                "AllKindsOfLights.ma", useTestSettings=False)
        cmds.refresh()

        #Lighting off
        cmds.modelEditor('modelPanel4', edit=True, displayLights='none')
        self.verifySnapshot("noLighting.png")
        
        #Use all lights
        cmds.modelEditor('modelPanel4', edit=True, displayLights='all')
        self.verifySnapshot("allLights.png")
        
        #Lighting default
        cmds.modelEditor('modelPanel4', edit=True, displayLights='default')
        self.verifySnapshot("defaultLighting.png")
        
    def test_MayaActiveLightingMode(self):
        testFile = mayaUtils.openTestScene(
                "testMayaLightingModes",
                "AllKindsOfLights.ma", useTestSettings=False)
        cmds.refresh()

        #Set active/selected lights only
        cmds.modelEditor('modelPanel4', edit=True, shadows=True)
        cmds.modelEditor('modelPanel4', edit=True, displayLights='active')
        cmds.select(clear=True)
        self.verifySnapshot("selLights_None.png")

        cmds.select("|stage1|stageShape1,/SphereLight1", replace=True)
        self.verifySnapshot("sphereLightSelected.png")

        cmds.select("pointLight1", replace=True)
        self.verifySnapshot("pointLightSelected.png")

        cmds.select("aiSkyDomeLight1", replace=True)
        self.verifySnapshot("domeLightSelected.png")

        cmds.select("spotLight1", replace=True)
        self.verifySnapshot("spotLightSelected.png")

        cmds.select("directionalLight1", replace=True)
        cmds.refresh()
        #test removed as there is a bug with shadows and directional lights logged as HYDRA-1727
        #self.assertSnapshotClose("dirLightSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        cmds.select("areaLight1", replace=True)
        self.verifySnapshot("areaLightSelected.png")

        cmds.select("|stage1|stageShape1,/SphereLight1", replace=True)
        cmds.select("pointLight1", add=True)
        cmds.select("aiSkyDomeLight1", add=True)
        self.verifySnapshot("someLightsSelected.png")
    
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
