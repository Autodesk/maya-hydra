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

class TestBackgroundColor(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def compareBackgroundColor(self, imageName):
        self.setViewport2Renderer()
        self.assertSnapshotClose(imageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        self.setHdStormRenderer()
        self.assertSnapshotClose(imageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_BackgroundColor(self):
        cmds.displayRGBColor('background', 0.5, 0, 0)
        self.compareBackgroundColor("background_red.jpg")
        cmds.displayRGBColor('background', 0, 0.5, 0)
        self.compareBackgroundColor("background_green.jpg")
        cmds.displayRGBColor('background', 0, 0, 0.5)
        self.compareBackgroundColor("background_blue.jpg")
        
        cmds.displayRGBColor('background', 0, 0, 0)
        self.compareBackgroundColor("background_black.jpg")
        cmds.displayRGBColor('background', 0.5, 0.5, 0.5)
        self.compareBackgroundColor("background_gray.jpg")
        cmds.displayRGBColor('background', 1, 1, 1)
        self.compareBackgroundColor("background_white.jpg")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
