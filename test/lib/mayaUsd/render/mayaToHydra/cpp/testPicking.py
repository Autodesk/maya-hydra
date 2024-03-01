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
import unittest

from testUtils import PluginLoaded

class TestPicking(mtohUtils.MtohTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setUp(self):
        super(TestPicking, self).setUp()
        self.setHdStormRenderer()
        cmds.refresh()

    def test_PickMayaMesh(self):
        cubeObjectName, _ = cmds.polyCube()
        cmds.move(1, 2, 3)
        cmds.select(clear=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, f="TestPicking.pickMesh")

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_PickUsdMesh(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib
        cubeObjectAndNodeNames = cmds.polyCube()
        cmds.move(1, 2, 3)
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cmds.mayaUsdDuplicate(cmds.ls(cubeObjectAndNodeNames, long=True)[0], psPathStr)
        cmds.delete(cubeObjectAndNodeNames)
        cmds.select(clear=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectAndNodeNames[0], f="TestPicking.pickMesh")

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_PickUsdImplicitSurface(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib
        from pxr import UsdGeom, Sdf
        cubeObjectName = "USDCube"
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(psPathStr).GetStage()
        UsdGeom.Cube.Define(stage, "/" + cubeObjectName)
        cmds.select(clear=True)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, f="TestPicking.pickMesh")

    def test_MarqueeSelection(self):
        cmds.polyPlane()
        cmds.move(-5, 0, 5)
        cmds.rotate(45, -25, -60)
        cmds.scale(10, 1, 10)
        cmds.polyCube()
        cmds.move(-2, 1.5, 3)
        cmds.rotate(-40, 50, -60)
        cmds.polyTorus()
        cmds.move(0.5, -2, -2)
        cmds.rotate(30, -5, -20)
        cmds.refresh()
        self.setBasicCam(10)
        cmds.select(clear=True)
        cmds.refresh()
        #self.assertSnapshotClose("marquee_before.png", 0, 0)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="TestPicking.marqueeSelection")
            cmds.refresh()
            #self.assertSnapshotClose("marquee_after.png", 0, 0)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
