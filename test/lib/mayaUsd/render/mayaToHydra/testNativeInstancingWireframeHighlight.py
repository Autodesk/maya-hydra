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

class TestNativeInstancingWireframeHighlight(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _stagePathSegment = "|instancedCubeHierarchies|instancedCubeHierarchiesShape"

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.3

    def loadUsdScene(self):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testNativeInstancingWireframeHighlight', 'instancedCubeHierarchies.usda')
        usdUtils.createStageFromFile(usdScenePath)
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 3, 3, 3, type='float3')

    def setUp(self):
        super(TestNativeInstancingWireframeHighlight, self).setUp()
        self.loadUsdScene()

    def test_InstanceSelection(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        instancePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_1"
        parentPath = self._stagePathSegment + "," + "/cubeHierarchies"

        instanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(instancePath))
        parentItem = ufe.Hierarchy.createItem(ufe.PathString.path(parentPath))

        sn.clear()
        sn.append(instanceItem)
        self.assertSnapshotClose("instanceSelection_direct.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(parentItem)
        self.assertSnapshotClose("instanceSelection_parent.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_PrototypeSelection(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        baseCubePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_1/baseCube"
        topCubePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_2/topCube"

        baseCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(baseCubePath))
        topCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(topCubePath))

        sn.clear()
        sn.append(baseCubeItem)
        self.assertSnapshotClose("prototypeSelection_baseCube.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.clear()
        sn.append(topCubeItem)
        self.assertSnapshotClose("prototypeSelection_topCube.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_InstanceWireframeColorChange(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        firstInstancePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_1"
        secondInstancePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_2"

        firstInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(firstInstancePath))
        secondInstanceItem = ufe.Hierarchy.createItem(ufe.PathString.path(secondInstancePath))

        sn.clear()
        sn.append(firstInstanceItem)
        self.assertSnapshotClose("instanceWireframeColorChange_before.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(secondInstanceItem)
        self.assertSnapshotClose("instanceWireframeColorChange_after.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
    
    def test_PrototypeWireframeColorChange(self):
        sn = ufe.GlobalSelection.get()
        sn.clear()

        firstBaseCubePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_1/baseCube"
        firstTopCubePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_1/topCube"
        secondTopCubePath = self._stagePathSegment + "," + "/cubeHierarchies/cubes_2/topCube"

        firstBaseCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(firstBaseCubePath))
        firstTopCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(firstTopCubePath))
        secondTopCubeItem = ufe.Hierarchy.createItem(ufe.PathString.path(secondTopCubePath))

        sn.clear()
        sn.append(firstBaseCubeItem)
        self.assertSnapshotClose("prototypeWireframeColorChange_firstBaseCubeItem.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(firstTopCubeItem)
        self.assertSnapshotClose("prototypeWireframeColorChange_firstTopCubeItem.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        sn.append(secondTopCubeItem)
        self.assertSnapshotClose("prototypeWireframeColorChange_secondTopCubeItem.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
