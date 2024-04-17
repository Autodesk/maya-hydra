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

class TestUsdStageFromFile(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    CUBE_PATH = "|cube|cubeShape,/cube"
    CONE_PATH = "|cube|cubeShape,/pCone1"

    def test_UsdStageFromFile(self):
        with PluginLoaded('mayaHydraCppTests'):

            usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')

            usdUtils.createStageFromFile(usdScenePath)

            cmds.mayaHydraCppTest(self.CUBE_PATH, f="TestHydraPrim.fromAppPath")

            # Use refresh() to pull on the DG to ensure all proxy shape stage
            # computations are done and proxy shape DG plugs are set clean.
            cmds.refresh()

            # Load a second USD scene file.  We should call populate only once,
            # and data in the second scene file should appear in the Hydra
            # scene index scene.

            populateCallsPre = cmds.mayaHydraInstruments("MayaUsdProxyShapeSceneIndex:NbPopulateCalls", q=True)

            usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cone.usda')

            cmds.setAttr('cubeShape.filePath', usdScenePath, type="string")

            populateCallsPost = cmds.mayaHydraInstruments("MayaUsdProxyShapeSceneIndex:NbPopulateCalls", q=True)

            self.assertEqual(populateCallsPre+1, populateCallsPost)

            cmds.mayaHydraCppTest(self.CONE_PATH, f="TestHydraPrim.fromAppPath")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
