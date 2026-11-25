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
# Smooth mesh preview's "cage + smooth" mode (displaySmoothMesh 1): the cage is
# drawn over the smoothed surface and follows the mode at runtime, as VP2 draws
# it. Covers both paths: the stock render items and the render item override
# (carriers), which draw the same images.
#
# Reference images per renderer in SmoothCageTest/<renderer>/.
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils


class TestSmoothCage(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # idiff thresholds: a pixel fails when a channel moves by more than 0.1
    # (about 25/255), and the image fails when more than 0.1% of its pixels do.
    # The cage covers more than 0.2% of the image, so a missing cage fails.
    IMAGEDIFF_FAIL_THRESHOLD = 0.1
    IMAGEDIFF_FAIL_PERCENT = 0.1

    def setUp(self):
        # Only with Maya's Global Hydra mode.
        if not mtohUtils.mayaHasHydraMode():
            self.skipTest("needs Maya's Global Hydra mode")
        super(TestSmoothCage, self).setUp()
        self.setTestRenderer()
        # A new scene with the test settings: grid off, so only the sphere and
        # its cage are in the image.
        mayaUtils.openNewScene()
        self.sphere = cmds.polySphere(subdivisionsX=8, subdivisionsY=6, name="cageSphere")[0]
        self.shape = cmds.listRelatives(self.sphere, shapes=True)[0]
        cmds.setAttr(self.shape + ".smoothLevel", 2)
        cmds.select(clear=True)
        self.setBasicCam(4)
        self.panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(self.panel, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False, displayTextures=False)

    def assertViewport(self, refImage, smoothMode=None):
        if smoothMode is not None:
            cmds.setAttr(self.shape + ".displaySmoothMesh", smoothMode)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose(refImage, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def test_cageDrawnInShadedPanel(self):
        self.assertViewport("smooth.png", 2)
        self.assertViewport("cageAndSmooth.png", 1)

    def test_cageFollowsTheMode(self):
        # Turning smooth preview off leaves no cage, and turning "cage + smooth"
        # back on draws the same cage.
        self.assertViewport("base.png", 0)
        self.assertViewport("cageAndSmooth.png", 1)
        self.assertViewport("base.png", 0)
        self.assertViewport("cageAndSmooth.png", 1)

    def test_hiddenMeshDrawsNoCage(self):
        self.assertViewport("cageAndSmooth.png", 1)
        cmds.setAttr(self.sphere + ".visibility", False)
        self.assertViewport("nothing.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
