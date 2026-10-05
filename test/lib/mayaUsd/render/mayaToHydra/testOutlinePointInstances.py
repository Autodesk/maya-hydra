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

# HYDRA-2456 : selecting point instances (the "Instances" pick mode) must only outline the selected
# instances, not every instance drawn by the instancer.

import maya.cmds as cmds

import fixturesUtils
import mtohUtils
import testUtils
import ufe

class TestOutlinePointInstances(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _stagePathSegment = "|nestedPointInstancers|nestedPointInstancersShape"

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def setUp(self):
        super(TestOutlinePointInstances, self).setUp()

        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Only the outline selection-highlight mode draws outlines.")

        import usdUtils
        usdScenePath = testUtils.getTestScene('testUsdPointInstances', 'nestedPointInstancers.usda')
        usdUtils.createStageFromFile(usdScenePath)
        self.modifyDefaultLightIntensityByUsdVersion()
        self.setBasicCam(15)
        cmds.refresh()

    def verifySnapshot(self, imageName):
        cmds.refresh(force=True)
        self.assertSnapshotClose(imageName,
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

    def createItem(self, usdPath):
        return ufe.Hierarchy.createItem(
            ufe.PathString.path(self._stagePathSegment + "," + usdPath))

    def test_InstanceSelection(self):
        parentInstancerPath = "/Root/ParentPointInstancer"
        # Instances 0 and 2 draw the cubes, 1 and 3 the pyramids.
        cubesInstance0 = self.createItem(parentInstancerPath + "/0")
        cubesInstance2 = self.createItem(parentInstancerPath + "/2")
        pyramidsInstance1 = self.createItem(parentInstancerPath + "/1")
        parentInstancer = self.createItem(parentInstancerPath)

        sn = ufe.GlobalSelection.get()

        # Only the 14 cubes of instance 0, in the lead color.
        sn.clear()
        sn.append(cubesInstance0)
        self.verifySnapshot("instance0.png")

        # Both rows of cubes. Instance 2 becomes the lead; instance 0 switches to the selected color,
        # although both are instances of the same rprims.
        sn.append(cubesInstance2)
        self.verifySnapshot("instances0and2.png")

        # Only the 4 pyramids of instance 1.
        sn.clear()
        sn.append(pyramidsInstance1)
        self.verifySnapshot("instance1.png")

        # Whole instancer: every cube and pyramid (unchanged behavior, no instance isolation).
        sn.clear()
        sn.append(parentInstancer)
        self.verifySnapshot("parentInstancer.png")

        # Back to an instance selection after a whole selection: the isolation must come back.
        sn.clear()
        sn.append(cubesInstance0)
        self.verifySnapshot("instance0.png")

        sn.clear()
        self.verifySnapshot("noSelection.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
