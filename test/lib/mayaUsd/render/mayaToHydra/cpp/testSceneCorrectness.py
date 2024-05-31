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

import testUtils
from testUtils import PluginLoaded

class TestSceneCorrectness(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')
        usdUtils.createStageFromFile(usdScenePath)
        self.setHdStormRenderer()

    def test_HydraFromUsdSceneCorrectness(self):
        self.loadUsdScene()
        cmds.select(all=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="HydraScene.testHydraSceneCorrectness")

    def test_HydraFromMayaSceneCorrectness(self):
        self.makeCubeScene(camDist=6)
        cmds.select(all=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="HydraScene.testHydraSceneCorrectness")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
