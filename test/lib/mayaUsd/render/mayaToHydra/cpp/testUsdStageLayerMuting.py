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

class TestUsdStageLayerMuting(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    CUBE_PRIM_PATH = "/USDCube"
    SUB_LAYER_CUBE_SIZE = 8.0

    def setSubLayerMuted(self, isMuted: bool):
        cmds.mayaUsdLayerEditor(self.subLayer.identifier, edit=True, muteLayer=(isMuted, self.usdProxyShapeUfePathString))

    def setupUsdStage(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, Sdf

        self.setHdStormRenderer()
        cmds.refresh()

        # Create a USD stage
        self.usdProxyShapeUfePathString = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(self.usdProxyShapeUfePathString).GetStage()

        # Define a cube prim in the stage (implicit surface)
        UsdGeom.Cube.Define(stage, self.CUBE_PRIM_PATH)

        # Add a sublayer to the stage
        rootLayer = stage.GetRootLayer()
        subLayerId = cmds.mayaUsdLayerEditor(rootLayer.identifier, edit=True, addAnonymous="testSubLayer")[0]
        self.subLayer = Sdf.Layer.Find(subLayerId)

        # Author an opinion on the sublayer
        stage.SetEditTarget(self.subLayer)
        cubePrim = stage.GetPrimAtPath(self.CUBE_PRIM_PATH)
        cubePrim.GetAttribute('size').Set(self.SUB_LAYER_CUBE_SIZE)

        cmds.refresh()

    def test_UsdStageLayerMuting(self):
        self.setupUsdStage()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="UsdStageLayerMuting.testSubLayerUnmuted")
            self.setSubLayerMuted(True)
            cmds.mayaHydraCppTest(f="UsdStageLayerMuting.testSubLayerMuted")
            self.setSubLayerMuted(False)
            cmds.mayaHydraCppTest(f="UsdStageLayerMuting.testSubLayerUnmuted")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
