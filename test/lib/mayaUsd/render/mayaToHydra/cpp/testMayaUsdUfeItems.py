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

import fixturesUtils
import mtohUtils
import unittest

from testUtils import PluginLoaded

class TestMayaUsdUfeItems(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setupUsdStage(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, UsdLux

        self.setHdStormRenderer()
        cmds.refresh()
        usdProxyShapeUfePathString = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdStage = mayaUsd.lib.GetPrim(usdProxyShapeUfePathString).GetStage()
        UsdGeom.Cylinder.Define(usdStage, "/USDCylinder")
        UsdLux.RectLight.Define(usdStage, "/USDRectLight")
        cmds.refresh()

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_SkipMayaUsdUfeLights(self):
        self.setupUsdStage()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="MayaUsdUfeItems.skipUsdUfeLights")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
