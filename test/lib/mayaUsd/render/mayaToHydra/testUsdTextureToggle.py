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

    IMAGE_DIFF_FAIL_THRESHOLD = 0.2
    IMAGE_DIFF_FAIL_PERCENT = 0.55

    def test_UsdTextureToggle(self):        

        # open simple Maya scene
        testFile = mayaUtils.openTestScene(
                "testUsdTextureToggle",
                "testUsdTextureToggle.ma", useTestSettings=False)
        cmds.refresh()
        
        panel = mayaUtils.activeModelPanel()
        self.assertSnapshotClose("usd_texture_on" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        cmds.refresh()      
        cmds.modelEditor(panel, edit=True, displayTextures=False)        
        self.assertSnapshotClose("usd_texture_off" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())