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
import platform
import maya.cmds as cmds

import fixturesUtils
import mayaUtils
import mtohUtils

class TestTransformMeshAdapter(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.2

    def verifySnapshot(self, imageName, imageVersion=None):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT,
                                 imageVersion=imageVersion)

    # def test_mayaMesh(self):
    #     self.makeCubeScene(camDist=6)
    #     cubeParent = cmds.group(self.cubeTrans, name='cubeParent')
    #     cmds.select(clear=1)
        
    #     self.verifySnapshot("cube_untransformed.png")

    #     cmds.scale(3, 1, 3, self.cubeTrans, absolute=True)
    #     self.verifySnapshot("cube_scaled.png")

    #     cmds.move(0, 2, 0, self.cubeTrans, absolute=True)
    #     self.verifySnapshot("cube_scaled_moved.png")

    #     cmds.rotate(0, 45, 0, self.cubeTrans, absolute=True)
    #     self.verifySnapshot("cube_scaled_moved_rotated.png")

    #     cmds.move(0, -3, 0, cubeParent, absolute=True)
    #     self.verifySnapshot("cube_parent_moved.png")

    #     cmds.rotate(0, 0, 45, cubeParent, absolute=True)
    #     self.verifySnapshot("cube_parent_moved_rotated.png")

    #     cmds.scale(2, 2, 2, cubeParent, absolute=True)
    #     self.verifySnapshot("cube_parent_moved_rotated_scaled.png")

    # When mesh adapter is enabled, NURBS curves should bypass NurbsCurveAdapter and only be rendered
    # via MayaHydraRenderItemAdapter. The test verifies there aren't unexpected curves shown by the NurbsCurveAdapter.
    def test_mayaNurbsCurve(self):
        mayaUtils.openNewScene()
        self.setBasicCam(5)
        self.setHdStormRenderer()

        # Use OSX-specific reference images on macOS
        imageVersion = 'osx' if platform.system() == 'Darwin' else None

        curveTrans = cmds.circle(name="mayaNurbsCurve")
        curveParent = cmds.group(curveTrans, name='curveParent')
        
        self.verifySnapshot("curve_untransformed.png", imageVersion=imageVersion)

        cmds.scale(3, 1, 3, curveTrans, absolute=True)
        self.verifySnapshot("curve_scaled.png", imageVersion=imageVersion)

        cmds.move(0, 2, 0, curveTrans, absolute=True)
        self.verifySnapshot("curve_scaled_moved.png", imageVersion=imageVersion)

        cmds.rotate(0, 45, 0, curveTrans, absolute=True)
        self.verifySnapshot("curve_scaled_moved_rotated.png", imageVersion=imageVersion)

        cmds.move(0, -3, 0, curveParent, absolute=True)
        self.verifySnapshot("curve_parent_moved.png", imageVersion=imageVersion)

        cmds.rotate(0, 0, 45, curveParent, absolute=True)
        self.verifySnapshot("curve_parent_moved_rotated.png", imageVersion=imageVersion)

        cmds.scale(2, 2, 2, curveParent, absolute=True)
        self.verifySnapshot("curve_parent_moved_rotated_scaled.png", imageVersion=imageVersion)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
