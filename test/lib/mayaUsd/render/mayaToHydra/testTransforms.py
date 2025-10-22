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

class TestTransforms(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    _requiredPlugins = ['drawUfe']

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 1

    def verifySnapshot(self, imageName, imageVersion=None):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT, imageVersion)

    def test_nativePrim(self):
        self.makeCubeScene(camDist=6)
        cubeParent = cmds.group(self.cubeTrans, name='cubeParent')
        cmds.select(clear=1)
        
        self.verifySnapshot("cube_untransformed.png")

        cmds.scale(3, 1, 3, self.cubeTrans, absolute=True)
        self.verifySnapshot("cube_scaled.png")

        cmds.move(0, 2, 0, self.cubeTrans, absolute=True)
        self.verifySnapshot("cube_scaled_moved.png")

        cmds.rotate(0, 45, 0, self.cubeTrans, absolute=True)
        self.verifySnapshot("cube_scaled_moved_rotated.png")

        cmds.move(0, -3, 0, cubeParent, absolute=True)
        self.verifySnapshot("cube_parent_moved.png")

        cmds.rotate(0, 0, 45, cubeParent, absolute=True)
        self.verifySnapshot("cube_parent_moved_rotated.png")

        cmds.scale(2, 2, 2, cubeParent, absolute=True)
        self.verifySnapshot("cube_parent_moved_rotated_scaled.png")

    def test_usdPrim(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, Gf

        mayaUtils.openNewScene()
        self.setBasicCam(10)
        self.setHdStormRenderer()

        # Create a USD stage
        usdProxyShapeUfePathString = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(usdProxyShapeUfePathString).GetStage()

        # Define a cube prim in the stage
        parentA = "/ParentA"
        childCube = "/ParentA/Cube"
        parentPrimA = stage.DefinePrim(parentA, 'Xform')
        childPrimCube = stage.DefinePrim(childCube, 'Cube')
        
        #modify light intensity for usd 24.11+
        self.modifyDefaultLightIntensityByUsdVersion()
        
        #Deal with 2 render passes
        img_version = None
        frame_passes_count = self.framePassesCount
        if frame_passes_count == 2:
            img_version = "two_passes"

        self.verifySnapshot("usd_cube_untransformed.png")

        UsdGeom.XformCommonAPI(childPrimCube).SetScale((2, 1, 2))
        self.verifySnapshot("usd_cube_scaled.png")

        UsdGeom.XformCommonAPI(childPrimCube).SetTranslate((0, 2, 0))
        self.verifySnapshot("usd_cube_scaled_moved.png")

        UsdGeom.XformCommonAPI(childPrimCube).SetRotate(Gf.Vec3f(0, 45, 0))
        self.verifySnapshot("usd_cube_scaled_moved_rotated.png")

        UsdGeom.XformCommonAPI(parentPrimA).SetTranslate((0, -4, 0))
        self.verifySnapshot("usd_cube_parent_moved.png")

        UsdGeom.XformCommonAPI(parentPrimA).SetRotate(Gf.Vec3f(0, 0, 45))
        self.verifySnapshot("usd_cube_parent_moved_rotated.png")

        UsdGeom.XformCommonAPI(parentPrimA).SetScale((2, 3, 2))
        self.verifySnapshot("usd_cube_parent_moved_rotated_scaled.png", img_version)

        self.resetDefaultLightIntensityByUsdVersion()

    def test_mayaCamera(self):
        mayaUtils.openNewScene()
        self.setBasicCam(10)
        self.setHdStormRenderer()

        camTrans, camShape = cmds.camera(name='testCam')

        self.verifySnapshot("camera_untransformed.png")

        cmds.move(0, 2, 0, camTrans, absolute=True)
        self.verifySnapshot("camera_moved.png")

        cmds.rotate(0, 45, 0, camTrans, absolute=True)
        self.verifySnapshot("camera_moved_rotated.png")

        cmds.scale(3, 1, 3, camTrans, absolute=True)
        self.verifySnapshot("camera_moved_rotated_scaled.png")

        camParent = cmds.group(camTrans, name='camParent')
        cmds.move(0, -3, 0, camParent, absolute=True)
        self.verifySnapshot("camera_parent_moved.png")

        cmds.rotate(0, 0, 45, camParent, absolute=True)
        self.verifySnapshot("camera_parent_moved_rotated.png")

        cmds.scale(2, 2, 2, camParent, absolute=True)
        self.verifySnapshot("camera_parent_moved_rotated_scaled.png")

    def test_mayaLight(self):
        mayaUtils.openNewScene()
        self.setBasicCam(10)
        self.setHdStormRenderer()

        lightShape = cmds.directionalLight(name='mayaDirectionalLightShape')
        lightTrans = cmds.listRelatives(lightShape, parent=True)[0]
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights='all')

        self.verifySnapshot("light_untransformed.png")

        cmds.move(2, 3, 2, lightTrans, absolute=True)
        self.verifySnapshot("light_moved.png")

        cmds.rotate(-20, 45, 30, lightTrans, absolute=True)
        self.verifySnapshot("light_moved_rotated.png")

        cmds.scale(3, 1, 3, lightTrans, absolute=True)
        self.verifySnapshot("light_moved_rotated_scaled.png")

        lightParent = cmds.group(lightTrans, name='lightParent')
        cmds.move(0, -3, 0, lightParent, absolute=True)
        self.verifySnapshot("light_parent_moved.png")

        cmds.rotate(0, 0, 45, lightParent, absolute=True)
        self.verifySnapshot("light_parent_moved_rotated.png")

        cmds.scale(1.5, 1.5, 1.5, lightParent, absolute=True)
        self.verifySnapshot("light_parent_moved_rotated_scaled.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
