#
# Copyright 2024 Autodesk, Inc. All rights reserved.
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
import ufe
import testUtils
import maya.cmds as cmds

class TestMeshWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _stagePathSegment = "|cubesHierarchy|cubesHierarchyShape"

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testMeshWireframeHighlight', 'cubesHierarchy.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestMeshWireframeHighlight, self).setUp()
        self.loadUsdScene()
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 2, 2, 2, type='float3')
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.refresh()

    def test_MeshSelection(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        baseCubePath = self._stagePathSegment + "," + "/parent/baseCube"
        parentPath = self._stagePathSegment + "," + "/parent"

        baseCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(baseCubePath))
        parentItem = ufe.Hierarchy.createItem(ufe.PathString.path(parentPath))

        sn.clear()
        sn.append(baseCubeItem)
        self.assertSnapshotClose("meshSelection_direct.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(parentItem)
        self.assertSnapshotClose("meshSelection_parent.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_WireframeColorChange(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        baseCubePath = self._stagePathSegment + "," + "/parent/baseCube"
        topCubePath = self._stagePathSegment + "," + "/parent/topCube"

        baseCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(baseCubePath))
        topCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(topCubePath))

        sn.clear()
        sn.append(baseCubeItem)
        self.assertSnapshotClose("wireframeColorChange_before.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(topCubeItem)
        self.assertSnapshotClose("wireframeColorChange_after.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
