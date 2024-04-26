# Copyright 2024 Autodesk
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
import usdUtils

import testUtils
from testUtils import PluginLoaded

class TestUsdAnim(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    CUBE_PATH = "|cubeAnim|cubeAnimShape,/Cube"

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testUsdAnim', 'cubeAnim.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestUsdAnim, self).setUp()
        self.loadUsdScene()
        cmds.refresh()

    def test_TimeVaryingTransform(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(self.CUBE_PATH, f="TestUsdAnim.timeVaryingTransform")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
