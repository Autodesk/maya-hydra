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

from testUtils import PluginLoaded

class TestSceneBrowser(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    SCENE_BROWSER_TEST_PLUGIN_NAME = 'mayaHydraSceneBrowserTest'
    SCENE_BROWSER_TEST_PLUGIN_COMMAND = SCENE_BROWSER_TEST_PLUGIN_NAME

    def setupScene(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, UsdLux
        
        self.setHdStormRenderer()
        cmds.refresh()
        usdProxyShapeUfePathString = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdStage = mayaUsd.lib.GetPrim(usdProxyShapeUfePathString).GetStage()
        UsdGeom.Cylinder.Define(usdStage, "/USDCylinder")
        UsdLux.RectLight.Define(usdStage, "/USDRectLight")
        cmds.polySphere(name="MayaTorus")
        cmds.directionalLight(name="MayaDirectionalLight")
        cmds.refresh()

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin() and mtohUtils.checkForPlugin(SCENE_BROWSER_TEST_PLUGIN_NAME), 
                         f'Requires mayaUSD and {SCENE_BROWSER_TEST_PLUGIN_NAME} plugins.')
    def test_SceneBrowser(self):
        self.setupScene()
        with PluginLoaded(self.SCENE_BROWSER_TEST_PLUGIN_NAME):
            mel.eval(self.SCENE_BROWSER_TEST_PLUGIN_COMMAND)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
