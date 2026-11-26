# Copyright 2026 Autodesk
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
# Display Map Border (polyOptions -displayMapBorder): in a shaded panel a mesh
# draws its UV border edges while it is selected, and not otherwise, as VP2
# draws them. Covers both paths: the stock render items and the render item
# override (carriers), which draw the same images.
#
# VP2 draws the border wider than the wire, in the wire's colour; Hydra draws
# lines one pixel wide. So the border is only visible where no wire is drawn on
# the same edges: a selected mesh in a shaded panel, with the outline selection
# highlight rather than a highlight wire. A wireframe panel (the mesh's wire)
# and "cage + smooth" mode (the cage wire) draw the border exactly over another
# line of the same colour, so they are not checked here.
#
# Reference images per renderer in MapBorderTest/<renderer>/.
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils


class TestMapBorder(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # idiff thresholds: a pixel fails when a channel moves by more than 0.1
    # (about 25/255), and the image fails when more than 0.3% of its pixels do.
    # A missing or extra border - the seam and the two poles, one pixel wide -
    # changes about 0.55% of the image, so it fails. Lines are antialiased
    # differently on macOS, which changes a fraction of their pixels (mostly
    # their alpha): the limit leaves room for that.
    IMAGEDIFF_FAIL_THRESHOLD = 0.1
    IMAGEDIFF_FAIL_PERCENT = 0.3

    def setUp(self):
        # Only with Maya's Global Hydra mode.
        if not mtohUtils.mayaHasHydraMode():
            self.skipTest("needs Maya's Global Hydra mode")
        super(TestMapBorder, self).setUp()
        # A highlight wire lies on the border's edges in the border's colour.
        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("the selection highlight is a wire, drawn over the map border")
        self.setTestRenderer()
        # A new scene with the test settings: grid off, so only the sphere and
        # its border are in the image.
        mayaUtils.openNewScene()
        # polySphere's UVs have a seam and two poles: it has a UV border.
        self.sphere = cmds.polySphere(subdivisionsX=12, subdivisionsY=8, name="borderSphere")[0]
        self.shape = cmds.listRelatives(self.sphere, shapes=True)[0]
        # A fully transparent surface: the border's edges lie exactly on the
        # faces, and nothing draws them in front (Hydra has no equivalent of VP2's
        # depth priority), so over an opaque surface they depth-fight with it. A
        # transparent surface writes no depth, so the border draws whole and the
        # same every time, the back half through the front.
        material = cmds.shadingNode("openPBRSurface", asShader=True, name="borderGlass")
        cmds.setAttr(material + ".geometryOpacity", 0.0)
        cmds.select(self.sphere)
        cmds.hyperShade(assign=material)
        cmds.select(clear=True)
        self.setBasicCam(4)
        self.panel = mayaUtils.activeModelPanel()
        # Textured mode, so the material's opacity is drawn.
        cmds.modelEditor(self.panel, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False, displayTextures=True)

    def assertViewport(self, refImage, border=None):
        if border is not None:
            cmds.polyOptions(self.shape, displayMapBorder=border)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose(refImage, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def test_unselectedMeshDrawsNoBorder(self):
        self.assertViewport("unselected.png", False)
        self.assertViewport("unselected.png", True)

    def test_selectedMeshDrawsBorder(self):
        cmds.select(self.sphere)
        self.assertViewport("selected.png", False)
        self.assertViewport("selectedBorder.png", True)

    def test_borderFollowsTheOption(self):
        # Turning Display Map Border off leaves no border, and turning it back on
        # draws the same one.
        cmds.select(self.sphere)
        self.assertViewport("selectedBorder.png", True)
        self.assertViewport("selected.png", False)
        self.assertViewport("selectedBorder.png", True)

    def test_borderFollowsTheSelection(self):
        cmds.polyOptions(self.shape, displayMapBorder=True)
        self.assertViewport("unselected.png")
        cmds.select(self.sphere)
        self.assertViewport("selectedBorder.png")
        cmds.select(clear=True)
        self.assertViewport("unselected.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
