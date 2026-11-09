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

from testUtils import PluginLoaded

class TestPicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setUp(self):
        super(TestPicking, self).setUp()
        self.setTestRenderer()
        cmds.refresh()

    def createMayaCube(self):
        objectName = cmds.polyCube()[0]
        cmds.move(1, 2, 3)
        cmds.select(clear=True)
        cmds.refresh()
        return objectName
    
    def createMayaDirectionalLight(self):
        shapeName = cmds.directionalLight()
        objectName = cmds.listRelatives(shapeName, parent=True)
        cmds.move(3, -2, 1)
        cmds.select(clear=True)
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights='all')
        cmds.refresh()
        return objectName

    def createUsdCubeFromMaya(self, stagePath):
        objectName = cmds.polyCube()[0]
        cmds.move(-4, 3, -2)
        cmds.mayaUsdDuplicate(cmds.ls(objectName, long=True)[0], stagePath)
        cmds.delete(objectName)
        cmds.select(clear=True)
        cmds.refresh()
        return objectName
    
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

    def createMayaCubeAt(self, translation):
        objectName = cmds.polyCube()[0]
        cmds.move(*translation)
        cmds.select(clear=True)
        cmds.refresh()
        return objectName

    def lookThroughOverscanPanZoomCamera(self):
        # Overscan and 2D pan/zoom exist only in Maya's projection matrix, which the pick uses. The
        # test objects are off-center so that a pick that ignores them misses.
        cameraTransform, cameraShape = cmds.camera()
        cmds.xform(cameraTransform, translation=[0, 2, 15])
        cmds.setAttr(cameraShape + '.overscan', 1.3)
        cmds.setAttr(cameraShape + '.panZoomEnabled', True)
        cmds.setAttr(cameraShape + '.horizontalPan', 0.3)
        cmds.setAttr(cameraShape + '.verticalPan', 0.1)
        cmds.setAttr(cameraShape + '.zoom', 0.6)
        cmds.lookThru(mayaUtils.activeModelPanel(), cameraTransform)
        cmds.select(clear=True)
        cmds.refresh()

    def test_PickMayaMesh(self):
        cubeObjectName = self.createMayaCube()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")

    def test_PickMayaLight(self):
        directionalLightObjectName = self.createMayaDirectionalLight()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(directionalLightObjectName, "simpleLight", f="TestPicking.pickObject")

    def test_PickUsdMesh(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cubeObjectName = self.createUsdCubeFromMaya(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")

    def test_PickUsdImplicitSurface(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cubeObjectName = self.createUsdCube(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")
    
    def test_PickUsdLight(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        rectLightObjectName = self.createUsdRectLight(stagePath)
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(rectLightObjectName, "rectLight", f="TestPicking.pickObject")

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

    # The pick must follow the camera's overscan and pan/zoom, as the draw does.
    def test_PickWithOverscanPanZoomCamera(self):
        self.lookThroughOverscanPanZoomCamera()
        cubeObjectName = self.createMayaCubeAt([5, 3, 0])
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(cubeObjectName, "mesh", f="TestPicking.pickObject")

    def test_MarqueeSelectionWithOverscanPanZoomCamera(self):
        self.lookThroughOverscanPanZoomCamera()
        # Keep both cubes near the view center, or a wide viewport clips one of them.
        firstCubeName = self.createMayaCubeAt([5, 3, 0])
        secondCubeName = self.createMayaCubeAt([2, 4, 0])
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                firstCubeName, "mesh",
                secondCubeName, "mesh",
                f="TestPicking.marqueeSelect")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
