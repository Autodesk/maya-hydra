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
import maya.cmds as cmds

import fixturesUtils
import mtohUtils

# What:   In outline mode, a shaded panel never gets a prim for a shape's VP2 selection
#         wireframe (DormantPolyWire), neither while the shape is selected nor after it is
#         deselected (HYDRA-2629).
# How:    Select and deselect a cube in a shaded panel, and list the render index after each
#         step. Then check the cases where the wire is needed: a wireframe panel, and a templated
#         shape, which is drawn as a wireframe.
# Expect: no wire prim in the shaded panel; a wire prim in both needed cases.
#
# Before the fix, the wire skipped while the cube was selected was translated on the
# deselection: a hidden prim with the cube's full topology, which made the first deselection of
# large scenes slow.
#
# Runs on the stock render item path (MAYAHYDRA_RENDER_ITEM_OVERRIDE=0, see CMakeLists.txt): a
# render item override carrier's wire is always translated, then hidden.
class TestUnneededHighlightWire(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def wirePrims(self):
        return [p for p in self.getIndex() if 'dormantpolywire' in p.lower()]

    def test_UnneededHighlightWire(self):
        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Outline mode only: legacy mode draws the wire as the highlight.")

        panel = self.activeEditor
        cmds.modelEditor(panel, edit=True, displayAppearance='smoothShaded',
                         wireframeOnShaded=False, activeOnly=False)
        cube = cmds.polyCube()[0]
        cmds.refresh(force=True)
        self.assertEqual(self.wirePrims(), [], "Wire prim before any selection.")

        cmds.select(cube, replace=True)
        cmds.refresh(force=True)
        self.assertEqual(self.wirePrims(), [],
                         "Wire prim while selected: the outline replaces the wire.")

        cmds.select(clear=True)
        cmds.refresh(force=True)
        self.assertEqual(self.wirePrims(), [],
                         "Wire prim after the deselection: VP2 does not draw it in a shaded "
                         "panel, so it must not be translated.")

        # Needed case 1: a wireframe panel draws the wire of every shape.
        cmds.modelEditor(panel, edit=True, displayAppearance='wireframe')
        cmds.refresh(force=True)
        self.assertEqual(len(self.wirePrims()), 1, "No wire prim in a wireframe panel.")

    def test_TemplatedShapeKeepsItsWire(self):
        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Outline mode only: legacy mode draws the wire as the highlight.")

        panel = self.activeEditor
        cmds.modelEditor(panel, edit=True, displayAppearance='smoothShaded',
                         wireframeOnShaded=False, activeOnly=False)
        cube = cmds.polyCube()[0]
        cmds.refresh(force=True)

        # Needed case 2: a templated shape is drawn as a wireframe, even in a shaded panel.
        cmds.setAttr(cube + '.template', 1)
        cmds.refresh(force=True)
        self.assertEqual(len(self.wirePrims()), 1,
                         "No wire prim for a templated shape: it would vanish from the viewport.")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
