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

class TestUsdTextureToggle(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 1.5

    def setUp(self):
        # Open simple Maya scene
        testFile = mayaUtils.openTestScene(
                "testUsdStageDefaultLighting",
                "testUsdStageDefaultLighting.ma")
        cmds.refresh()
    
    def setTextureMode(self, enabled):
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayTextures=enabled)        
        cmds.refresh()

    def setCameraTransform(self, xtrans, ytrans, ztrans, xrot, yrot, zrot ):
        #Move camera
        cmds.setAttr('persp.translate', xtrans, ytrans, ztrans, type='float3')
        cmds.setAttr('persp.rotate', xrot, yrot, zrot, type='float3')

    def test_UsdStageDefaultLighting(self):
        self.setHdStormRenderer()        
        self.setTextureMode(True)

        self.setCameraTransform(-21, 7, 18, -20, -50, 0)
        self.assertSnapshotClose("usdStageDefaultLighting_1" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        self.setCameraTransform(20, 12, 16, -30, 50, 0)
        self.assertSnapshotClose("usdStageDefaultLighting_2" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())