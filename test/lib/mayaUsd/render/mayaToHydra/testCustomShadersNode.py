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

class TestCustomShadersNode(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 2 #We have errors on Windows and Linux of about 1%, so we need to increase the threshold

    def test_LoadCustomShaderNode(self):
        with PluginLoaded('mayaHydraCustomShadersNode'):
            testFile = mayaUtils.openTestScene( 
                "testCustomShadersNode",
                "testCustomShadersNode.ma")
            cmds.refresh()
            self.assertSnapshotClose("testCustomShadersNodeDefaultLight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            # Switch the lighting mode to use all scene lights.
            cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights = 'all')
            self.assertSnapshotClose("testCustomShadersNodeUseAllLights.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

            #Remove the direct lighting to check if the dome light works fine
            cmds.setAttr("pointLightShape1.intensity", 0);
            self.assertSnapshotClose("testCustomShadersNodeDomeLightOnly.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
