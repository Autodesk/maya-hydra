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
import mayaUtils
import mtohUtils
import testUtils
import usdUtils

from testUtils import PluginLoaded

class TestBoundingBox(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _extentsFilename = "cube_extents.usda"
    _noExtentsFilename = "cube_no_extents.usda"
    
    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def setUp(self):
        super(TestBoundingBox, self).setUp()

    def loadUsdScene(self, stageFilename):
        usdScenePath = testUtils.getTestScene('testBoundingBox', stageFilename)
        usdUtils.createStageFromFile(usdScenePath)
        cmds.select(clear=True)
        self.setBasicCam(5)
        self.modifyDefaultLightIntensityByUsdVersion()
        panel = mayaUtils.activeModelPanel()        
        cmds.modelEditor(panel, edit=True, displayAppearance="boundingBox")
        cmds.refresh()

    def test_BoundingBoxWithExtents(self):
        self.loadUsdScene(self._extentsFilename)
        self.assertSnapshotClose("boundingBox_extents.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_BoundingBoxNoExtents(self):
        self.loadUsdScene(self._noExtentsFilename)
        self.assertSnapshotClose("boundingBox_no_extents.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
