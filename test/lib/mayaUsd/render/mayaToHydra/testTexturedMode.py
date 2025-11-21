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

class TestTexturedMode(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1.0

    def test_TexturedMode(self):
        """
        Load in a Maya file with a USD stage with a textured sphere,
        then toggle the viewport textured mode setting.
        """
        testFile = mayaUtils.openTestScene(
            "testTexturedMode", "texturedUsdSphere.ma")

        self.setHdStormRenderer()
        self.setBasicCam(5)

        cmds.refresh()

        self.assertSnapshotClose(
            "untextured.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        panel = 'modelPanel4'
        cmds.modelEditor(panel, edit=True, displayTextures=True)

        self.assertSnapshotClose(
            "textured.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

        cmds.modelEditor(panel, edit=True, displayTextures=False)

        self.assertSnapshotClose(
            "untextured.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT
        )

if __name__ == '__main__':
    fixturesUtils.runTests(globals())

