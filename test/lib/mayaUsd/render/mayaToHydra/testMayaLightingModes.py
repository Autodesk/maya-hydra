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

class TestMayaLightingfModes(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']
    
    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.5
    
    def test_MayaUsualLightingModes(self):
        # open the Maya scene
        testFile = mayaUtils.openTestScene(
                "testMayaLightingModes",
                "AllKindsOfLights.ma", useTestSettings=False)
        cmds.refresh()
        
        #Lighting off
        cmds.modelEditor('modelPanel4', edit=True, displayLights='none')
        self.assertSnapshotClose("noLighting.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        #Use all lights
        cmds.modelEditor('modelPanel4', edit=True, displayLights='all')
        self.assertSnapshotClose("allLights.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        #Lighting default
        cmds.modelEditor('modelPanel4', edit=True, displayLights='default')
        self.assertSnapshotClose("defaultLighting.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
    def test_MayaActiveLightingMode(self):
        testFile = mayaUtils.openTestScene(
                "testMayaLightingModes",
                "AllKindsOfLights.ma", useTestSettings=False)
        cmds.refresh()
        
        #Set active/selected lights only
        cmds.modelEditor('modelPanel4', edit=True, displayLights='active')
        cmds.select(clear=True)
        cmds.refresh()
        self.assertSnapshotClose("selLights_None.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.select("|stage1|stageShape1,/SphereLight1", replace=True)
        cmds.refresh()
        self.assertSnapshotClose("sphereLightSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.select("pointLight1", replace=True)
        cmds.refresh()
        self.assertSnapshotClose("pointLightSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.select("aiSkyDomeLight1", replace=True)
        cmds.refresh()
        self.assertSnapshotClose("domeLightSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        cmds.select("|stage1|stageShape1,/SphereLight1", replace=True)
        cmds.select("pointLight1", add=True)
        cmds.select("aiSkyDomeLight1", add=True)
        cmds.refresh()
        self.assertSnapshotClose("allLightsSelected.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
