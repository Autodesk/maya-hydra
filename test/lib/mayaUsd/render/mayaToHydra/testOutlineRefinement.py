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

# The outline of a selected mesh must follow the surface Storm draws, which is the subdivided
# limit surface when the mesh is refined, not its control cage.

import os

import maya.cmds as cmds

import fixturesUtils
import mtohUtils
import testUtils

REFINEMENT_LEVEL_ATTR_NAME = "mayaHydraRefinementLevel"
REFINEMENT_LEVEL_ATTR = "defaultRenderGlobals.{}".format(REFINEMENT_LEVEL_ATTR_NAME)

# High enough for the limit surface of a cube to sit well inside the cube's silhouette.
REFINED_LEVEL = 3

def _usesMeshAdapter():
    return bool(os.getenv('MAYA_HYDRA_USE_MESH_ADAPTER'))

class TestOutlineRefinement(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    # This script is also registered with MAYA_HYDRA_USE_MESH_ADAPTER=1. The two runs can execute
    # in parallel, and each one deletes its output directory on startup, so they need separate ones.
    @classmethod
    def selectionHighlightOutputSuffix(cls):
        suffix = super(TestOutlineRefinement, cls).selectionHighlightOutputSuffix()
        return '_meshAdapter' + suffix if _usesMeshAdapter() else suffix

    def setUp(self):
        super(TestOutlineRefinement, self).setUp()

        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Only the outline selection-highlight mode draws outlines.")

        self.modifyDefaultLightIntensityByUsdVersion()

    def verifySnapshot(self, imageName):
        cmds.refresh(force=True)
        self.assertSnapshotClose(imageName,
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

    def setRefinementLevel(self, level):
        # The render override only re-reads the plug on 'mayaHydra -updateRenderGlobals'.
        cmds.setAttr(REFINEMENT_LEVEL_ATTR, level)
        cmds.mayaHydra(updateRenderGlobals=REFINEMENT_LEVEL_ATTR_NAME)

    def test_usdMesh(self):
        # The USD path does not depend on the mesh adapter: run it once.
        if _usesMeshAdapter():
            self.skipTest("Covered by the run without MAYA_HYDRA_USE_MESH_ADAPTER.")

        import usdUtils
        # No authored subdivisionScheme, so the cube is Catmull-Clark and refines to a rounded shape.
        usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')
        usdUtils.createStageFromFile(usdScenePath)
        proxyShape = cmds.ls(type="mayaUsdProxyShape", long=True)[0]

        self.setBasicCam(1.5)
        cmds.select(proxyShape + ",/cube")

        try:
            self.setRefinementLevel(0)
            self.verifySnapshot("usdCube_refinementLevel0.png")

            self.setRefinementLevel(REFINED_LEVEL)
            self.verifySnapshot("usdCube_refinementLevel3.png")

            # Back to the cage: the outline must follow it again.
            self.setRefinementLevel(0)
            self.verifySnapshot("usdCube_refinementLevel0.png")
        finally:
            # The render override keeps the refinement level across scenes.
            self.setRefinementLevel(0)

    def test_mayaMeshSmoothPreview(self):
        # On the mesh adapter (opt-in), Smooth Mesh Preview reaches Hydra as a refine level, and
        # the renderer refines the mesh (the case being tested). On the default render item path,
        # VP2 hands over already smoothed geometry at refine level 0 (a control: the outline must
        # match there too).
        cubeTransform = cmds.polyCube()[0]
        cubeShape = cmds.listRelatives(cubeTransform, shapes=True)[0]
        # Same as pressing "3" in the viewport.
        cmds.setAttr(cubeShape + ".displaySmoothMesh", 2)
        cmds.setAttr(cubeShape + ".smoothLevel", REFINED_LEVEL)

        self.setBasicCam(1.5)
        cmds.select(cubeTransform)

        imageName = ("mayaCube_smoothPreview_meshAdapter.png" if _usesMeshAdapter()
                     else "mayaCube_smoothPreview_renderItem.png")
        self.verifySnapshot(imageName)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
