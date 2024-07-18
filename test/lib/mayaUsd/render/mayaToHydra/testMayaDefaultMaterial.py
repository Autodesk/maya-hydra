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

class TestMayaDefaultMaterial(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 1.5

    def test_MayaDefaultMaterial(self):

        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            # open a Maya scene with maya mesh, usd prims and custom scene indices prims
            testFile = mayaUtils.openTestScene(
                    "testDefaultMaterial",
                    "testMayaDefaultMaterial_Native_Usd_dataProducer.ma")
            cmds.refresh()
            panel = mayaUtils.activeModelPanel()
            self.assertSnapshotClose("sceneLoaded" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)        
        
            #Use Default Material
            cmds.modelEditor(panel, edit=True, useDefaultMaterial=True)
            cmds.refresh()
            self.assertSnapshotClose("defaultMaterial" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)        
    
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
