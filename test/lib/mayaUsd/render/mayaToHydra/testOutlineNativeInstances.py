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

# HYDRA-2456 : selecting native instances must outline the selected instances, and only them. A
# native instance prim has no rprim under it: the instancer UsdImaging adds for it draws its
# prototype elsewhere, for every instance.

import maya.cmds as cmds

import fixturesUtils
import mtohUtils
import testUtils
import ufe

class TestOutlineNativeInstances(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def setUp(self):
        super(TestOutlineNativeInstances, self).setUp()

        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Only the outline selection-highlight mode draws outlines.")

    def loadUsdScene(self, sceneName, center, dist):
        import usdUtils
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', sceneName + '.usda')
        usdUtils.createStageFromFile(usdScenePath)
        self._stagePathSegment = "|" + sceneName + "|" + sceneName + "Shape"
        self.modifyDefaultLightIntensityByUsdVersion()
        # setBasicCam(), aimed at center.
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate',
                     center[0] + dist, center[1] + 0.75 * dist, center[2] + dist,
                     type='float3')
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
        # Two native instances of a base cube with a top cube: cubes_1 at x = -1, cubes_2 at x = 1.
        self.loadUsdScene("instancedCubeHierarchies", (0, 0.75, 0), 4)

        cubes1 = self.createItem("/cubeHierarchies/cubes_1")
        cubes2 = self.createItem("/cubeHierarchies/cubes_2")
        cubeHierarchies = self.createItem("/cubeHierarchies")
        cubes1BaseCube = self.createItem("/cubeHierarchies/cubes_1/baseCube")

        sn = ufe.GlobalSelection.get()

        # Only the two cubes of cubes_1, in the lead color.
        sn.clear()
        sn.append(cubes1)
        self.verifySnapshot("cubes1.png")

        # Both instances: cubes_2 becomes the lead, cubes_1 switches to the selected color,
        # although both are instances of the same rprims.
        sn.append(cubes2)
        self.verifySnapshot("cubes1and2.png")

        # The parent of the instances: both instances, in the lead color.
        sn.clear()
        sn.append(cubeHierarchies)
        self.verifySnapshot("cubeHierarchies.png")

        # A prim in the prototype, through one instance: only the base cube of cubes_1.
        sn.clear()
        sn.append(cubes1BaseCube)
        self.verifySnapshot("cubes1BaseCube.png")

        sn.clear()
        self.verifySnapshot("noSelection.png")

    def test_PointInstancesUnderNativeInstances(self):
        # HYDRA-2592 scene: rows NI_0, NI_1, NI_2 at z = 0, 4, 8, each the three point instances of
        # a cube at x = 0, 3, 6.
        self.loadUsdScene("nativeInstancedPointInstancers", (3, 0, 4), 14)

        nativeInstance1 = self.createItem("/Root/NI_1")
        pointInstance1OfNativeInstance1 = self.createItem("/Root/NI_1/PI/1")

        sn = ufe.GlobalSelection.get()

        # The three cubes of the middle row.
        sn.clear()
        sn.append(nativeInstance1)
        self.verifySnapshot("nativeInstance1.png")

        # Only the middle cube of the middle row: the selection keeps the native instance level
        # above the point instance one.
        sn.clear()
        sn.append(pointInstance1OfNativeInstance1)
        self.verifySnapshot("pointInstance1OfNativeInstance1.png")

        sn.clear()
        self.verifySnapshot("pointInstancersNoSelection.png")

    def test_NestedNativeInstances(self):
        # Rows Outer_0, Outer_1, Outer_2 at z = 0, 4, 8, each the native instances Inner_0, Inner_1,
        # Inner_2 of a cube at x = 0, 3, 6.
        self.loadUsdScene("nestedNativeInstances", (3, 0, 4), 14)

        outer1 = self.createItem("/Root/Outer_1")
        inner1OfOuter1 = self.createItem("/Root/Outer_1/Inner_1")
        cubeOfInner1OfOuter1 = self.createItem("/Root/Outer_1/Inner_1/Cube")

        sn = ufe.GlobalSelection.get()

        # The three cubes of the middle row: they are drawn under the inner instancer, itself
        # drawn by the outer one.
        sn.clear()
        sn.append(outer1)
        self.verifySnapshot("outer1.png")

        # Only the middle cube of the middle row: a native instance nested in a prototype.
        sn.clear()
        sn.append(inner1OfOuter1)
        self.verifySnapshot("inner1OfOuter1.png")

        # The same cube, selected as the prim in the inner prototype: the selection crosses two
        # native instances and keeps both levels, so the same image.
        sn.clear()
        sn.append(cubeOfInner1OfOuter1)
        self.verifySnapshot("inner1OfOuter1.png")

        sn.clear()
        self.verifySnapshot("nestedNoSelection.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
