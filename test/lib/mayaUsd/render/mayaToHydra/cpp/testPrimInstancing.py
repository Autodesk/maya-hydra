# Copyright 2023 Autodesk
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
import maya.mel as mel

import fixturesUtils
import mtohUtils
import unittest

import testUtils
from testUtils import PluginLoaded

class TestPrimInstancing(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupUsdStage(self):
        # Load the USD scene
        usdScenePath = testUtils.getTestScene('testPrimInstancing', 'scene.usda')
        mel.eval('source \"mayaUsd_createStageFromFile.mel\"')
        loadStageCmd = f'mayaUsd_createStageFromFilePath("{usdScenePath}")'
        loadStageCmd = loadStageCmd.replace('\\', '/')
        mel.eval(loadStageCmd)
        self.setHdStormRenderer()
        cmds.refresh()

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_UsdPrimInstancing(self):
        self.setupUsdStage()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="PrimInstancing.testUsdPrimInstancing")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
