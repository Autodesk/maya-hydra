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
import mayaUtils

from pxr import UsdGeom, Gf

import unittest

class TestUsdDeleteCamera(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # The drawUfe plugin is required as it draws UFE (and thus USD) cameras.
    _requiredPlugins = ['drawUfe']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.0

    @unittest.skipUnless(mayaUtils.hydraFixLevel() > 3, "Requires UFE MRenderItem camera delete bug fix.")
    def test_UsdDeleteCamera(self):
        """
        Test that creates a USD stage with a single USD camera, verifies it is
        shown in the Hydra Storm viewport, then deletes the USD stage and
        verifies the camera is no longer shown.
        """
        self.setHdStormRenderer()
        self.setBasicCam(dist=5)

        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib

        # Create a USD stage
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(psPathStr).GetStage()

        # Create a USD camera prim.
        cameraPrim = stage.DefinePrim('/Camera', 'Camera')

        cmds.refresh(force=True)

        # Compare the viewport render with a reference image showing the USD camera
        self.assertSnapshotClose(
            "usdCameraPresent.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        # Delete the USD stage by deleting the proxy shape transform node
        transformNode = "stage1"
        cmds.delete(transformNode)

        # Refresh the viewport to update the render
        cmds.refresh()

        # Compare the viewport render with a reference image showing no USD camera
        self.assertSnapshotClose(
            "usdCameraAbsent.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

if __name__ == '__main__':
    fixturesUtils.runTests(globals())

