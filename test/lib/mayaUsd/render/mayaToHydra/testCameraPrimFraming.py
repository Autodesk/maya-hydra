# Copyright 2026 Autodesk
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

# Image-comparison tests where we check that the viewport still frames the scene the way Viewport 2.0
# does. We focus on the Maya camera settings that only exist in Maya's projection matrix,
# which the solved display window has to reproduce.
class TestCameraPrimFraming(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2

    def setupScene(self):
        self.setTestRenderer()

        # Off-center objects of distinct shapes, so that any framing error moves or scales them.
        cubeName = cmds.polyCube()[0]
        cmds.xform(cubeName, translation=[-3, 0, 0])
        sphereName = cmds.polySphere()[0]
        cmds.xform(sphereName, translation=[3, 1, 0])
        coneName = cmds.polyCone()[0]
        cmds.xform(coneName, translation=[0, 3, -2])

        # The default 36x24 film back never matches the square test viewport, so every film
        # fit mode frames differently.
        self.cameraTransform, self.cameraShape = cmds.camera()
        cmds.xform(self.cameraTransform, translation=[0, 2, 15])
        cmds.lookThru(mayaUtils.activeModelPanel(), self.cameraTransform)
        cmds.select(clear=True)
        cmds.refresh()

    def setCameraAttr(self, attr, value):
        cmds.setAttr(self.cameraShape + '.' + attr, value)

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotAndCompareVp2(
            imageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_FilmFit(self):
        self.setupScene()
        for fitIndex, fitName in enumerate(['fill', 'horizontal', 'vertical', 'overscan']):
            self.setCameraAttr('filmFit', fitIndex)
            self.verifySnapshot('filmFit_' + fitName + '.png')

    def test_Overscan(self):
        self.setupScene()
        self.setCameraAttr('overscan', 1.3)
        self.verifySnapshot('overscan.png')

    def test_PanZoom(self):
        self.setupScene()
        self.setCameraAttr('panZoomEnabled', True)
        self.setCameraAttr('horizontalPan', 0.3)
        self.setCameraAttr('verticalPan', -0.2)
        self.setCameraAttr('zoom', 0.6)
        self.verifySnapshot('panZoom.png')

    def test_FilmTranslate(self):
        self.setupScene()
        self.setCameraAttr('horizontalFilmOffset', 0.2)
        self.setCameraAttr('filmTranslateH', 0.15)
        self.setCameraAttr('filmTranslateV', -0.1)
        self.verifySnapshot('filmTranslate.png')

    def test_ScaledCamera(self):
        self.setupScene()
        # VP2 ignores the camera transform's own scale, so the image must match the unscaled
        # camera's default Horizontal fit.
        cmds.xform(self.cameraTransform, scale=[2, 1, 0.5])
        self.verifySnapshot('filmFit_horizontal.png')

    def test_RenderRegion(self):
        self.setupScene()
        self.setCameraAttr('overscan', 1.2)
        self.setCameraAttr('panZoomEnabled', True)
        self.setCameraAttr('horizontalPan', 0.2)
        self.setCameraAttr('zoom', 0.8)

        # The region must crop the full-viewport framing, not squeeze it into the region. The test
        # viewport is 400x400.
        cmds.mayaHydraRenderRegion(edit=True, region=[50, 50, 300, 250])
        try:
            cmds.refresh()
            self.assertSnapshotClose(
                'renderRegion.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        finally:
            cmds.mayaHydraRenderRegion(edit=True, clear=True)

    def test_UsdCamera(self):
        import mayaUsd.lib
        import mayaUsd_createStageWithNewLayer

        self.setupScene()
        self.setCameraAttr('filmFit', 1)
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdCameraPaths = mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(self.cameraTransform, long=True)[0], stagePath)
        self.assertTrue(usdCameraPaths)
        usdCameraPath = usdCameraPaths[0] if isinstance(usdCameraPaths, (list, tuple)) else usdCameraPaths

        # Look through the USD camera, then move the Maya camera away to prove it is not used.
        cmds.lookThru(mayaUtils.activeModelPanel(), usdCameraPath)
        cmds.xform(self.cameraTransform, translation=[20, 20, 20])
        cmds.select(clear=True)
        cmds.refresh()
        self.assertSnapshotClose(
            'usdCamera.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def setupOrthoScene(self):
        self.setupScene()
        cmds.setAttr(self.cameraShape + '.orthographic', True)
        cmds.setAttr(self.cameraShape + '.orthographicWidth', 14)

    def test_OrthoTopCamera(self):
        self.setupScene()
        cmds.lookThru(mayaUtils.activeModelPanel(), 'top')
        cmds.refresh()
        self.verifySnapshot('ortho_top.png')

    def test_OrthoCamera(self):
        self.setupOrthoScene()
        self.verifySnapshot('ortho_camera.png')

    def test_OrthoWidth(self):
        self.setupOrthoScene()
        self.verifySnapshot('ortho_camera.png')
        # Zooming an orthographic camera changes its width, which must reach the camera prim.
        self.setCameraAttr('orthographicWidth', 9)
        self.verifySnapshot('ortho_width.png')

    def test_OrthoOverscan(self):
        self.setupOrthoScene()
        self.setCameraAttr('overscan', 1.3)
        self.verifySnapshot('ortho_overscan.png')

    def test_OrthoPanZoom(self):
        self.setupOrthoScene()
        self.setCameraAttr('panZoomEnabled', True)
        self.setCameraAttr('horizontalPan', 2.0)
        self.setCameraAttr('verticalPan', -1.0)
        self.setCameraAttr('zoom', 0.6)
        self.verifySnapshot('ortho_panZoom.png')

    def test_OrthoShake(self):
        self.setupOrthoScene()
        self.setCameraAttr('shakeEnabled', True)
        self.setCameraAttr('horizontalShake', 1.5)
        self.setCameraAttr('verticalShake', -1.0)
        # Maya ignores the film offset for orthographic cameras.
        self.setCameraAttr('horizontalFilmOffset', 0.5)
        self.verifySnapshot('ortho_shake.png')

    def test_OrthoCameraScale(self):
        self.setupOrthoScene()
        self.setCameraAttr('cameraScale', 1.5)
        self.verifySnapshot('ortho_cameraScale.png')

    def test_OrthoRenderRegion(self):
        self.setupOrthoScene()
        self.setCameraAttr('overscan', 1.2)
        self.setCameraAttr('panZoomEnabled', True)
        self.setCameraAttr('horizontalPan', 1.0)
        self.setCameraAttr('zoom', 0.8)

        cmds.mayaHydraRenderRegion(edit=True, region=[50, 50, 300, 250])
        try:
            cmds.refresh()
            self.assertSnapshotClose(
                'ortho_renderRegion.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        finally:
            cmds.mayaHydraRenderRegion(edit=True, clear=True)

    def test_UsdOrthoCamera(self):
        import mayaUsd.lib
        import mayaUsd_createStageWithNewLayer

        self.setupOrthoScene()
        # Maya frames USD orthographic cameras with the proxy camera shape's orthographic width,
        # which does not follow the USD camera's aperture (EMSUSD-1393). Hydra follows Maya's
        # matrix so that it matches VP2 picking, so this image changes once that is fixed.
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdCameraPaths = mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(self.cameraTransform, long=True)[0], stagePath)
        self.assertTrue(usdCameraPaths)
        usdCameraPath = usdCameraPaths[0] if isinstance(usdCameraPaths, (list, tuple)) else usdCameraPaths

        cmds.lookThru(mayaUtils.activeModelPanel(), usdCameraPath)
        cmds.xform(self.cameraTransform, translation=[20, 20, 20])
        cmds.select(clear=True)
        cmds.refresh()
        self.assertSnapshotClose(
            'ortho_usdCamera.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
