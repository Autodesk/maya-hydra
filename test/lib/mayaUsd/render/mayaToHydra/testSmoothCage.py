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
# (carriers).
#
# Compares snapshots of the same scene within the run rather than against
# baselines: what differs between two modes is exactly what the mode adds.
import os

import maya.cmds as cmds
import fixturesUtils
import imageUtils
import mayaUtils
import mtohUtils

try:
    from PySide6.QtGui import QImage
except ImportError:
    from PySide2.QtGui import QImage


def _loadPixels(path):
    image = QImage(path).convertToFormat(QImage.Format_RGBA8888)
    data = bytes(image.constBits())
    stride = image.bytesPerLine()
    return image.width(), image.height(), data, stride


def _differingPixels(pathA, pathB, tolerance=24):
    wa, ha, a, sa = _loadPixels(pathA)
    wb, hb, b, sb = _loadPixels(pathB)
    if (wa, ha) != (wb, hb):
        raise AssertionError("snapshot sizes differ: %s vs %s" % ((wa, ha), (wb, hb)))
    count = 0
    for y in range(ha):
        ra = a[y * sa:y * sa + 4 * wa]
        rb = b[y * sb:y * sb + 4 * wb]
        if ra == rb:
            continue
        for x in range(0, 4 * wa, 4):
            if (abs(ra[x] - rb[x]) > tolerance or abs(ra[x + 1] - rb[x + 1]) > tolerance
                    or abs(ra[x + 2] - rb[x + 2]) > tolerance):
                count += 1
    return count


class TestSmoothCage(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    SIZE = 400

    def setUp(self):
        super(TestSmoothCage, self).setUp()
        self.setTestRenderer()
        cmds.file(new=True, force=True)
        self.sphere = cmds.polySphere(subdivisionsX=8, subdivisionsY=6, name="cageSphere")[0]
        self.shape = cmds.listRelatives(self.sphere, shapes=True)[0]
        cmds.setAttr(self.shape + ".smoothLevel", 2)
        cmds.select(clear=True)
        self.setBasicCam(4)
        self.panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(self.panel, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False, displayTextures=False)
        self.snapDir = os.path.join(os.path.abspath("."), "smoothCageSnapshots")

    def snap(self, name, smoothMode=None):
        if smoothMode is not None:
            cmds.setAttr(self.shape + ".displaySmoothMesh", smoothMode)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        path = os.path.join(self.snapDir, name + ".png")
        imageUtils.snapshot(path, width=self.SIZE, height=self.SIZE)
        return path

    def test_cageDrawnInShadedPanel(self):
        smooth = self.snap("smooth", 2)
        cage   = self.snap("cageAndSmooth", 1)
        # The cage is the difference: the surface under it is the same smoothed
        # surface in both modes.
        self.assertGreater(_differingPixels(smooth, cage), 0.002 * self.SIZE * self.SIZE,
                           "the cage of 'cage + smooth' mode is not drawn")

    def test_cageFollowsTheMode(self):
        base1 = self.snap("base1", 0)
        cage1 = self.snap("cage1", 1)
        base2 = self.snap("base2", 0)
        cage2 = self.snap("cage2", 1)
        self.assertEqual(_differingPixels(base1, base2), 0, "turning smooth preview off leaves a cage")
        self.assertEqual(_differingPixels(cage1, cage2), 0, "turning 'cage + smooth' back on differs")

    def test_hiddenMeshDrawsNoCage(self):
        empty = self.snap("empty", 1)
        cmds.setAttr(self.sphere + ".visibility", False)
        hidden = self.snap("hidden")
        cmds.delete(self.sphere)
        nothing = self.snap("nothing")
        self.assertEqual(_differingPixels(hidden, nothing), 0, "a hidden mesh still draws its cage")
        self.assertGreater(_differingPixels(empty, nothing), 0)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
