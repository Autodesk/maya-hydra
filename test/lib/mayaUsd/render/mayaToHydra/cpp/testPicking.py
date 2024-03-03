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
import mayaUtils
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

    def createMayaCube(self):
        objectAndNodeNames = cmds.polyCube()
        cmds.move(1, 2, 3)
        cmds.select(clear=True)
        cmds.refresh()
        return objectAndNodeNames[0]
    
    def createMayaDirectionalLight(self):
        shapeName = cmds.directionalLight()
        objectName = cmds.listRelatives(shapeName, parent=True)
        cmds.move(3, -2, 1)
        cmds.select(clear=True)
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights='all')
        cmds.refresh()
        return objectName

    def createUsdCubeFromMaya(self, stagePath):
        objectAndNodeNames = cmds.polyCube()
        cmds.move(-4, 3, -2)
        cmds.mayaUsdDuplicate(cmds.ls(objectAndNodeNames, long=True)[0], stagePath)
        cmds.delete(objectAndNodeNames)
        cmds.select(clear=True)
        cmds.refresh()
        return objectAndNodeNames[0]
    
    def createUsdCube(self, stagePath):
        import mayaUsd.lib
        from pxr import UsdGeom
        objectName = "USDCube"
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        xform = UsdGeom.Xform.Define(stage, "/" + objectName + "Xform")
        xform.AddTranslateOp().Set(value=(6, 5, 4))
        UsdGeom.Cube.Define(stage, str(xform.GetPath()) + "/" + objectName)
        cmds.select(clear=True)
        cmds.refresh()
        return objectName
    
    def createUsdRectLight(self, stagePath):
        import mayaUsd.lib
        from pxr import UsdGeom, UsdLux
        objectName = "USDRectLight"
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        xform = UsdGeom.Xform.Define(stage, "/" + objectName + "Xform")
        xform.AddTranslateOp().Set(value=(-6, -3.5, -1))
        UsdLux.RectLight.Define(stage, str(xform.GetPath()) + "/" + objectName)
        cmds.select(clear=True)
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights='all')
        cmds.refresh()
        return objectName

    def test_PickMayaMesh(self):
        cubeObjectName = self.createMayaCube()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")

    def test_PickMayaLight(self):
        lightObjectName = self.createMayaDirectionalLight()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(lightObjectName, "simpleLight", f="TestPicking.pickObject")

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_PickUsdMesh(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cubeObjectName = self.createUsdCubeFromMaya(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_PickUsdImplicitSurface(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cubeObjectName = self.createUsdCube(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")
    
    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_PickUsdLight(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        lightObjectName = self.createUsdRectLight(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(lightObjectName, "rectLight", f="TestPicking.pickObject")

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_MarqueeSelection(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        mayaCubeName = self.createMayaCube()
        mayaDirectionalLightName = self.createMayaDirectionalLight()
        usdMayaCubeName = self.createUsdCubeFromMaya(stagePath)
        usdCubeName = self.createUsdCube(stagePath)
        usdRectLightName = self.createUsdRectLight(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                mayaCubeName, "mesh",
                mayaDirectionalLightName, "simpleLight",
                usdMayaCubeName, "mesh",
                usdCubeName, "mesh",
                usdRectLightName, "rectLight",
                f="TestPicking.marqueeSelect")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
