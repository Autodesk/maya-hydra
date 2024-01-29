# Copyright 2020 Luma Pictures
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
import maya.mel

import fixturesUtils
import mtohUtils
import unittest

class TestTransforms(mtohUtils.MtohTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1

    def setUp(self):
        self.imageVersion = None
        if maya.mel.eval("defaultShaderName") != "standardSurface1":
            self.imageVersion = 'lambertDefaultMaterial'

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT, 
                                 imageVersion=self.imageVersion)

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

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_usdPrim(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, Gf

        cmds.file(new=True, force=True)
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
        self.verifySnapshot("usd_cube_parent_moved_rotated_scaled.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
