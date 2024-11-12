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

class TestUsdStageInvalidate(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    def setupScene(self):
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', 'instancedCubeHierarchies.usda')
        usdUtils.createStageFromFile(usdScenePath)
        # Clear the selection, otherwise we'll get selection highlighting
        # geometry prim removals for the current stage when we create and
        # switch the selection to the new stage in the C++ part of the test.
        cmds.select(clear=True)
        cmds.refresh()

    def test_addStage(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="TestUsdStageInvalidate.testAddStage")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
