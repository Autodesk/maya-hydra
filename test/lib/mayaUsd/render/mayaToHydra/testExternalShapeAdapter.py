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
# An adapter in a library other than mayaHydraLib: the external shape adapter
# example (lib/mayaHydra/flowViewportAPIExamples/externalShapeAdapter). Nothing
# loads its library but MayaHydra, which finds it through its plugInfo.json; the
# Maya plug-in loaded here only has the node the adapter translates, a locator
# that draws nothing by itself.
#
# The node seen from straight above: the adapter draws it as a blue square.
#
# Reference images per renderer in ExternalShapeAdapterTest/<renderer>/.
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils


class TestExternalShapeAdapter(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # The Maya plug-in with the node. It does not load the adapter's library.
    _requiredPlugins = ['mhExternalShapeAdapterExampleNode.py']

    # idiff thresholds: a pixel fails when a channel moves by more than 0.1
    # (about 25/255), and the image fails when more than 1% of its pixels do.
    # The square is about 4% of the image.
    IMAGEDIFF_FAIL_THRESHOLD = 0.1
    IMAGEDIFF_FAIL_PERCENT = 1

    def setUp(self):
        super(TestExternalShapeAdapter, self).setUp()
        self.setTestRenderer()
        mayaUtils.openNewScene()
        # The test camera straight above the origin, looking down.
        cmds.setAttr("persp.rotate", -90, 0, 0, type="float3")
        cmds.setAttr("persp.translate", 0, 20, 0, type="float3")
        cmds.modelEditor(self.activeEditor, edit=True, displayAppearance="smoothShaded",
                         wireframeOnShaded=False)
        cmds.refresh(force=True)

    def assertViewport(self, refImage):
        cmds.refresh(force=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose(refImage, self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

    def test_externalShapeAdapter(self):
        # The node: its adapter draws a square.
        shape = cmds.createNode("MhExternalShapeAdapterExample")
        transform = cmds.listRelatives(shape, parent=True)[0]
        cmds.select(clear=True)
        self.assertViewport("square.png")
        # Moved: the square follows the node.
        cmds.setAttr(transform + ".translateX", 4)
        self.assertViewport("moved.png")
        # Deleted: the square is gone.
        cmds.delete(transform)
        self.assertViewport("deleted.png")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
