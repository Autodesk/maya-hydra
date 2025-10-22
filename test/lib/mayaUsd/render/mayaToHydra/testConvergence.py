# Copyright 2025 Autodesk
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

class TestConvergence(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def setUp(self):
        super(TestConvergence, self).setUp()
        self.setHdStormRenderer()
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
    
    def test_HdStormConvergence(self):
        # Create a scene with varied data
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        self.createMayaCube()
        self.createMayaDirectionalLight()
        self.createUsdCube(stagePath)
        self.createUsdCubeFromMaya(stagePath)
        self.createUsdRectLight(stagePath)

        # Refresh the viewport
        cmds.refresh(force=True)
        
        # Check that rendering has converged
        self.assertTrue(cmds.mayaHydraTesting(converged=True))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
