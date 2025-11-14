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
import mtohUtils
import mayaUtils

class TestUsdIncludedPurposes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.0

    def test_UsdIncludedPurposes(self):
        """
        Load in a Maya file with a USD stage, then 
        Test that creates a USD stage with a single USD camera, verifies it is
        shown in the Hydra Storm viewport, then deletes the USD stage and
        verifies the camera is no longer shown.
        """
        testFile = mayaUtils.openTestScene(
            "testUsdIncludedPurposes", "includedPurposesTest.ma")

        self.setHdStormRenderer()

        cmds.setAttr('defaultRenderGlobals.mayaHydraRenderPurpose', 1)
        cmds.setAttr('defaultRenderGlobals.mayaHydraGuidePurpose',  1)
        cmds.setAttr('defaultRenderGlobals.mayaHydraProxyPurpose',  1)

        cmds.refresh()

        self.assertSnapshotClose(
            "allPurposes.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        # Select the cone's parent.  Cone is render purpose.
        cmds.select('|stage1|stageShape1,/Xform2')

        cmds.refresh()

        self.assertSnapshotClose(
            "coneSelected.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        # HYDRA-1908: removing an included purpose must not show selection
        # highlighting for objects with that purpose.

        cmds.setAttr('defaultRenderGlobals.mayaHydraRenderPurpose', 0)

        cmds.refresh()

        self.assertSnapshotClose(
            "noRenderPurpose.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        cmds.setAttr('defaultRenderGlobals.mayaHydraProxyPurpose', 0)

        cmds.refresh()

        # At this point only USD object with no purpose and Maya object are
        # displayed.

        self.assertSnapshotClose(
            "onlyGuidePurpose.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

if __name__ == '__main__':
    fixturesUtils.runTests(globals())

