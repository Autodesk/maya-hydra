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
import mayaUtils


class TestImagePlanes(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def test_ImagePlanePlayback(self):
        mayaUtils.openTestScene(
            "testImagePlanes",
            "imagePlane.ma", useTestSettings=False)
        cmds.refresh()

        for frame in range(1, 11):
            cmds.currentTime(frame)
            self.assertSnapshotClose(
                "imagePlane_frame{}.png".format(frame),
                self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
