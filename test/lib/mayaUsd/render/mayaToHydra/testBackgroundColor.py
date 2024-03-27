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
import fixturesUtils
import mtohUtils

class TestBackgroundColor(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def test_BackgroundColor(self):
        self.assertSnapshotClose("background_red.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(0.5, 0, 0))
        self.assertSnapshotClose("background_green.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(0, 0.5, 0))
        self.assertSnapshotClose("background_blue.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(0, 0, 0.5))
        
        self.assertSnapshotClose("background_black.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(0, 0, 0))
        self.assertSnapshotClose("background_gray.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(0.5, 0.5, 0.5))
        self.assertSnapshotClose("background_white.jpg", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, backgroundColor=(1, 1, 1))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
