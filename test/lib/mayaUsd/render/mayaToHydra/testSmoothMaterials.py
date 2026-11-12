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
# Per-face materials on a smooth mesh preview. Maya assigns shading groups to
# the BASE polygons, and smooth preview draws the subdivided mesh: each base
# polygon's material has to follow onto the faces subdivided from it, as VP2's
# per-material render items do. Covers both paths: the stock render items and
# the render item override (carriers), which draw the same images.
#
# A plane seen from straight above, its left half red and its right half blue.
# Each case changes the same mesh, so the change has to reach the renderer.
#
# Reference images per renderer in SmoothMaterialsTest/<renderer>/.
import maya.cmds as cmds
import fixturesUtils
import mtohUtils


class TestSmoothMaterials(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # idiff thresholds: a pixel fails when a channel moves by more than 0.1
    # (about 25/255), and the image fails when more than 1% of its pixels do.
    # A half of the plane in the wrong colour is far above that.
    IMAGEDIFF_FAIL_THRESHOLD = 0.1
    IMAGEDIFF_FAIL_PERCENT = 1

    def makeMaterial(self, name, colour):
        # Incandescent, so the colours read the same lit or not.
        material = cmds.shadingNode("lambert", asShader=True, name=name)
        cmds.setAttr(material + ".color", *colour, type="double3")
        cmds.setAttr(material + ".incandescence", *colour, type="double3")
        shadingGroup = cmds.sets(renderable=True, noSurfaceShader=True, empty=True,
                                 name=name + "SG")
        cmds.connectAttr(material + ".outColor", shadingGroup + ".surfaceShader")
        # shadingNode leaves the new material selected, a selection the viewport
        # cannot map to a prim.
        cmds.select(clear=True)
        return shadingGroup

    def setUp(self):
        super(TestSmoothMaterials, self).setUp()
        self.setTestRenderer()
        cmds.file(new=True, force=True)
        # 4 x 2 faces: faces 0-1 and 4-5 are the left half, 2-3 and 6-7 the right.
        self.plane = cmds.polyPlane(width=12, height=6, subdivisionsX=4, subdivisionsY=2,
                                    name="smoothPlane")[0]
        self.shape = cmds.listRelatives(self.plane, shapes=True)[0]
        self.red = self.makeMaterial("smoothRed", (1.0, 0.0, 0.0))
        self.blue = self.makeMaterial("smoothBlue", (0.0, 0.0, 1.0))
        self.assign(self.red, [0, 1, 4, 5])
        self.assign(self.blue, [2, 3, 6, 7])

        # The test camera straight above the plane, looking down at its front.
        cmds.setAttr("persp.rotate", -90, 0, 0, type="float3")
        cmds.setAttr("persp.translate", 0, 20, 0, type="float3")
        cmds.modelEditor(self.activeEditor, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False)
        cmds.refresh(force=True)

    def assign(self, shadingGroup, faces):
        cmds.sets(["%s.f[%d]" % (self.plane, f) for f in faces], edit=True,
                  forceElement=shadingGroup)

    def setSmooth(self, mode, level=2):
        # displaySmoothMesh: 0 base mesh, 1 cage + smooth, 2 smooth only.
        cmds.setAttr(self.shape + ".displaySmoothMesh", mode)
        cmds.setAttr(self.shape + ".smoothLevel", level)

    def assertViewport(self, refImage):
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose(refImage, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def test_smoothMaterials(self):
        # The base mesh: two colours, split down the middle.
        self.assertViewport("base.png")
        # Smoothed: the same split, on the subdivided faces.
        self.setSmooth(2)
        self.assertViewport("smooth.png")
        # Another level: more faces, the same split.
        self.setSmooth(2, level=3)
        self.assertViewport("smoothLevel3.png")
        # An assignment changed while smoothed: a blue face turns red.
        self.assign(self.red, [2])
        self.assertViewport("smoothReassigned.png")
        # Back to the base mesh, with the new assignment.
        self.setSmooth(0)
        self.assertViewport("baseReassigned.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
