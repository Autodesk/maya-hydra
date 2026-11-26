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
# Face culling of a mesh, as VP2 culls its shaded items
# (OGSMayaSurfaceShapeEvaluator): .doubleSided off culls the back faces,
# .opposite culls the front faces instead, and a .backfaceCulling other than Off
# culls the back faces of a double-sided mesh too. Covers both paths: the stock
# render items and the render item override (carriers), which draw the same
# images.
#
# Each case changes the attributes on the same mesh, a plane seen from straight
# above, so the change has to reach the renderer, not only the first build.
#
# Reference images per renderer in CullingTest/<renderer>/.
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils
import rendererUtils


class TestCulling(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # idiff thresholds: a pixel fails when a channel moves by more than 0.1
    # (about 25/255), and the image fails when more than 1% of its pixels do.
    # A culled plane is the whole middle of the image, far above that.
    IMAGEDIFF_FAIL_THRESHOLD = 0.1
    IMAGEDIFF_FAIL_PERCENT = 1

    # backfaceCulling enum values (polyOptions / mesh.backfaceCulling).
    CULL_OFF = 0
    CULL_KEEP_WIRE = 1
    CULL_KEEP_HARD_EDGES = 2
    CULL_FULL = 3

    def setUp(self):
        super(TestCulling, self).setUp()
        self.setTestRenderer()
        # A new scene with the test settings: grid off, as the plane lies in the
        # grid's plane and the two would tie on depth.
        mayaUtils.openNewScene()
        # A plane facing +Y, seen from above: the camera sees its front faces.
        self.plane = cmds.polyPlane(width=10, height=10, subdivisionsX=1, subdivisionsY=1,
                                    name="cullPlane")[0]
        self.shape = cmds.listRelatives(self.plane, shapes=True)[0]
        # Red, so it stands out from the grey background, lit from either side or not.
        material = cmds.shadingNode("lambert", asShader=True, name="cullRed")
        cmds.setAttr(material + ".color", 1.0, 0.0, 0.0, type="double3")
        cmds.setAttr(material + ".incandescence", 1.0, 0.0, 0.0, type="double3")
        cmds.select(self.plane)
        cmds.hyperShade(assign=material)
        cmds.select(clear=True)

        # The test camera straight above the plane, looking down at its front.
        cmds.setAttr("persp.rotate", -90, 0, 0, type="float3")
        cmds.setAttr("persp.translate", 0, 20, 0, type="float3")
        cmds.modelEditor(self.activeEditor, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False)
        cmds.refresh(force=True)

    def assertCase(self, refImage, facingCamera, doubleSided, opposite=False,
                   backfaceCulling=CULL_OFF):
        cmds.setAttr(self.plane + ".rotateX", 0 if facingCamera else 180)
        cmds.setAttr(self.shape + ".doubleSided", doubleSided)
        cmds.setAttr(self.shape + ".opposite", opposite)
        cmds.setAttr(self.shape + ".backfaceCulling", backfaceCulling)
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose(refImage, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def test_culling(self):
        # Double-sided (the default): both sides are drawn.
        self.assertCase("frontDoubleSided.png", facingCamera=True, doubleSided=True)
        self.assertCase("backDoubleSided.png", facingCamera=False, doubleSided=True)
        # Single-sided: the back faces are culled.
        self.assertCase("backSingleSided.png", facingCamera=False, doubleSided=False)
        self.assertCase("frontSingleSided.png", facingCamera=True, doubleSided=False)
        # Opposite: the front faces are culled instead. That takes the prim's cull
        # style (front), which HdFlash does not honour on either path: it culls by
        # doubleSided alone, so a single-sided mesh always loses its back faces.
        if rendererUtils.getTestRenderer() != rendererUtils.TEST_RENDERER_FLASH:
            self.assertCase("frontOpposite.png", facingCamera=True, doubleSided=False,
                            opposite=True)
            self.assertCase("backOpposite.png", facingCamera=False, doubleSided=False,
                            opposite=True)
        # Backface culling culls the back faces of a double-sided mesh.
        self.assertCase("backCullFull.png", facingCamera=False, doubleSided=True,
                        backfaceCulling=self.CULL_FULL)
        self.assertCase("backCullKeepWire.png", facingCamera=False, doubleSided=True,
                        backfaceCulling=self.CULL_KEEP_WIRE)
        self.assertCase("frontCullFull.png", facingCamera=True, doubleSided=True,
                        backfaceCulling=self.CULL_FULL)
        # And back to drawing both sides.
        self.assertCase("backCullOff.png", facingCamera=False, doubleSided=True,
                        backfaceCulling=self.CULL_OFF)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
