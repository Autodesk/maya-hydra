# Copyright 2025 Autodesk
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
import testUtils
import usdUtils

class TestUsdInheritsSpecializes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _requiredPlugins = ['mayaHydraCppTests']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def loadUsdScene(self, fileName):

        # Move the camera in closer
        self.setBasicCam(5)

        usdScenePath = testUtils.getTestScene('testUsdInheritsSpecializes', fileName)
        usdUtils.createStageFromFile(usdScenePath)

    def test_Inherits(self):

        self.loadUsdScene('cubeInherits.usda')

        cmds.refresh()

        self.assertSnapshotClose('inherits.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_Specializes(self):

        self.loadUsdScene('cubeSpecializes.usda')

        cmds.refresh()

        # Same snapshot as inherits.
        self.assertSnapshotClose('inherits.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
