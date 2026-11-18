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
import maya.mel as mel

import fixturesUtils
import imageDiffUtils
import mayaUtils
import mtohUtils

# Image-comparison tests for the camera exposure attribute (EV stops), which reaches the
# render delegate as HdCameraSchema::linearExposureScale = 2^exposure.
# The baselines are Flash only (HdStorm is skipped in CMakeLists.txt).
class TestCameraExposure(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2

    # The panels of the four-view layout used by the multi-panel test.
    PERSP_PANEL = 'modelPanel4'
    SECOND_PANEL = 'modelPanel1'

    def setupScene(self):
        self.setTestRenderer()

        # A white cube under the default light. The default gray material renders too dark
        # for a one-stop change to stand out; white keeps -1 to +1 EV well clear of both
        # black and white.
        cubeName = cmds.polyCube()[0]
        cmds.xform(cubeName, rotation=[30, 45, 0])
        shader = cmds.shadingNode('lambert', asShader=True)
        cmds.setAttr(shader + '.color', 1, 1, 1, type='double3')
        cmds.setAttr(shader + '.diffuse', 1)
        cmds.select(cubeName)
        cmds.hyperShade(assign=shader)

        self.cameraTransform, self.cameraShape = cmds.camera()
        cmds.xform(self.cameraTransform, translation=[0, 0, 6])
        # USD cameras have no film fit and match horizontally, so use the same fit for every
        # Maya camera and the USD camera test can share the Maya camera baselines.
        self.setCameraAttr('filmFit', 1)
        cmds.lookThru(mayaUtils.activeModelPanel(), self.cameraTransform)
        cmds.select(clear=True)
        cmds.refresh()

    def setCameraAttr(self, attr, value, cameraShape=None):
        cmds.setAttr((cameraShape or self.cameraShape) + '.' + attr, value)

    def exposureImage(self, exposure):
        return {-1: 'exposure_minus1.png', 0: 'exposure_0.png', 1: 'exposure_plus1.png'}[exposure]

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(
            imageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def assertBaselinesDiffer(self, image1, image2):
        # Guards against baselines regenerated while exposure had no effect, which would
        # make every exposure test pass without testing anything.
        result = imageDiffUtils.compareImagePair(
            self.resolveRefImage(image1, None), self.resolveRefImage(image2, None),
            self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT,
            diffOutputDir=self.getSnapshotDir(), passReturnCodes=(0, 1))
        self.assertFalse(
            result.passed, image1 + ' and ' + image2 + ' should differ: exposure has no effect')

    # What: exposure must brighten or darken the image by whole stops.
    # How: render the same view at -1, 0 and +1 EV.
    # Expect: each image matches its baseline, and the baselines differ from each other.
    def test_ExposureSweep(self):
        self.setupScene()
        for exposure in (-1, 0, 1):
            self.setCameraAttr('exposure', exposure)
            self.verifySnapshot(self.exposureImage(exposure))

        self.assertBaselinesDiffer(self.exposureImage(0), self.exposureImage(-1))
        self.assertBaselinesDiffer(self.exposureImage(0), self.exposureImage(1))

    # What: exposure must only affect the viewport that looks through that camera.
    # How: two panels look through two cameras with the same view, one at +1 EV and the
    # other at 0 EV.
    # Expect: each panel matches the baseline of its own camera's exposure.
    def test_ExposureOnlyAffectsItsViewport(self):
        self.setupScene()
        secondCameraTransform, secondCameraShape = cmds.camera()
        cmds.xform(secondCameraTransform, translation=[0, 0, 6])
        self.setCameraAttr('filmFit', 1, secondCameraShape)

        self.setCameraAttr('exposure', 1)
        self.setCameraAttr('exposure', 0, secondCameraShape)

        mel.eval('FourViewLayout')
        try:
            for panel, cameraTransform in (
                    (self.PERSP_PANEL, self.cameraTransform),
                    (self.SECOND_PANEL, secondCameraTransform)):
                cmds.setFocus(panel)
                self.setTestRenderer()
                cmds.lookThru(panel, cameraTransform)
                # The two cameras share a position, so each would see the other's gizmo.
                cmds.modelEditor(panel, edit=True, cameras=False, grid=False)
            cmds.select(clear=True)

            cmds.setFocus(self.PERSP_PANEL)
            self.verifySnapshot(self.exposureImage(1))
            cmds.setFocus(self.SECOND_PANEL)
            self.verifySnapshot(self.exposureImage(0))
        finally:
            cmds.setFocus(self.PERSP_PANEL)
            mel.eval('SingleViewArrangement')

    # What: a Maya camera must look the same as an equivalent USD camera with the same
    # exposure.
    # How: duplicate the Maya camera to USD, author exposure 1 on it and look through it.
    # Expect: the USD camera image matches the Maya camera's +1 EV baseline.
    def test_ExposureMatchesUsdCamera(self):
        import mayaUsd.lib
        import mayaUsd.ufe
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom

        self.setupScene()
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        usdCameraPaths = mayaUsd.lib.PrimUpdaterManager.duplicate(
            cmds.ls(self.cameraTransform, long=True)[0], stagePath)
        self.assertTrue(usdCameraPaths)
        usdCameraPath = usdCameraPaths[0] if isinstance(usdCameraPaths, (list, tuple)) else usdCameraPaths

        # Leave the other exposure:* attributes at their defaults, so that
        # UsdGeomCamera::ComputeLinearExposureScale reduces to 2^exposure.
        usdCamera = UsdGeom.Camera(mayaUsd.ufe.ufePathToPrim(usdCameraPath))
        self.assertTrue(usdCamera)
        usdCamera.CreateExposureAttr().Set(1.0)

        # Look through the USD camera, then move the Maya camera away to prove it is not used.
        cmds.lookThru(mayaUtils.activeModelPanel(), usdCameraPath)
        cmds.xform(self.cameraTransform, translation=[20, 20, 20])
        cmds.select(clear=True)
        self.verifySnapshot(self.exposureImage(1))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
