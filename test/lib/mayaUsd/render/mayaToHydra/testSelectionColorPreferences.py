#
# Copyright 2025 Autodesk, Inc. All rights reserved.
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
import maya.mel as mel

import fixturesUtils
import mayaUtils
import mtohUtils

class TestSelectionColorPreferences(mtohUtils.MayaHydraBaseTestCase):
    '''A colour preference edit must repaint what is already on screen.

    MtohRenderOverride::ColorPreferencesChanged() only sets dirty flags; the outline
    style rebuild, MhWireframeColorInterfaceImp::RefreshColors() and the prim
    invalidation all happen on the next Render(). Nothing else asserts that, and the
    invalidation is split by reach -- the two selection colours reach only selected
    prims and the *WhSi highlight prims, polymeshDormant reaches every prim -- so a
    test has to cover both halves or an inverted split passes.

    Deliberately never re-selects between the edit and the capture: a selection change
    dirties the prims by itself and would mask the bug this guards.
    '''

    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2

    # Maya preference names, see kLeadColorName / kPolymeshActiveColorName /
    # kPolymeshDormantColorName in lib/mayaHydra/hydraExtensions/mayaUtils.h.
    LEAD_PREF = "lead"
    ACTIVE_PREF = "polymeshActive"
    DORMANT_PREF = "polymeshDormant"

    def setUp(self):
        super(TestSelectionColorPreferences, self).setUp()
        # makeCubeScene() re-applies the selection highlighting mode itself.
        self.makeCubeScene(camDist=8)
        self.secondCube = cmds.polyCube()[0]
        cmds.setAttr(self.secondCube + '.translateX', 3)
        cmds.select(clear=True)
        cmds.refresh(force=True)

    def tearDown(self):
        mel.eval("displayRGBColor -rf; displayColor -rf; colorIndex -rf;")
        super(TestSelectionColorPreferences, self).tearDown()

    def setPref(self, name, r, g, b):
        '''Change a colour preference and hand the viewport one frame to react.

        No selection change in between: the repaint has to come from
        ColorPreferencesChanged()'s flags alone.
        '''
        cmds.displayRGBColor(name, r, g, b)
        cmds.refresh(force=True)

    def test_leadAndActiveColorsApplyImmediately(self):
        # Two objects selected, so the second is the lead and the first is active:
        # both selection colours are on screen at once.
        cmds.select([self.cubeTrans, self.secondCube], replace=True)
        cmds.refresh(force=True)
        self.assertSnapshotClose("selectionColors_default.png",
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

        self.setPref(self.LEAD_PREF, 1.0, 0.0, 1.0)
        self.setPref(self.ACTIVE_PREF, 0.0, 1.0, 1.0)
        self.assertSnapshotClose("selectionColors_edited.png",
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

    def test_dormantColorAppliesImmediately(self):
        # Wireframe: the display style that actually pulls polymeshDormant. Nothing
        # selected, so this exercises the other side of the reach split -- the
        # every-prim colour, with no selected prim involved.
        cmds.select(clear=True)
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True,
                         displayAppearance='wireframe')
        cmds.refresh(force=True)
        self.assertSnapshotClose("dormantColor_default.png",
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

        self.setPref(self.DORMANT_PREF, 1.0, 0.5, 0.0)
        self.assertSnapshotClose("dormantColor_edited.png",
                                 self.IMAGE_DIFF_FAIL_THRESHOLD,
                                 self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())