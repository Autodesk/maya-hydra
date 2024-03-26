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

class TestUsdNativeInstancePicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    PICK_PATH = "|instancedCubeHierarchies|instancedCubeHierarchiesShape,/cubeHierarchies"

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', 'instancedCubeHierarchies.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestUsdNativeInstancePicking, self).setUp()
        self.loadUsdScene()
        cmds.refresh()

    def test_NativeInstances(self):
        with PluginLoaded('mayaHydraCppTests'):
            instances = ["/cubes_1", "/cubes_2"]
            for instance in instances:
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + instance,
                    f="TestUsdNativeInstancePicking.pickInstance")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
