#
# Copyright 2026 Autodesk, Inc. All rights reserved.
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

class TestCameraOverscan(mtohUtils.MayaHydraBaseTestCase):
    """Checks that the camera overscan is applied to the Hydra viewport.

    Overscan is not a camera prim parameter: non-Storm render delegates receive it through the
    framing. Viewport 2.0 is the reference, so no baseline image is needed: the test compares the
    screen-space bounds of a cube rendered by the test renderer against those rendered by VP2.
    """

    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.5
    IMAGE_DIFF_FAIL_PERCENT = 1

    TEST_OVERSCANS = [0.5, 1.0, 1.5]

    def setUp(self):
        super(TestCameraOverscan, self).setUp()
        cube = cmds.polyCube()[0]
        cmds.xform(cube, scale=[4, 4, 4], translation=[1, 0, -2])
        self.setBasicCam(10)
        cmds.select(clear=True)
        cmds.refresh()

    def test_Overscan(self):
        for overscan in self.TEST_OVERSCANS:
            cmds.setAttr('perspShape.overscan', overscan)
            self.assertSnapshotClose(
                "overscan_{}.png".format(overscan),
                self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
