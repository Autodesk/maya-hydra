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
    '''A colour preference edit must repaint what is already on screen on the next frame.

    The selection colours dirty only selected prims and highlight prims, while
    polymeshDormant dirties every prim, so both cases are tested. The selection is
    never changed between edit and capture, since that would dirty the prims by itself.
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
        # makeCubeScene() opens a new scene and re-applies the highlighting mode.
        self.makeCubeScene(camDist=8)
        self.secondCube = cmds.polyCube()[0]
        cmds.setAttr(self.secondCube + '.translateX', 3)
        cmds.select(clear=True)
        cmds.refresh(force=True)

    def tearDown(self):
        mel.eval("displayRGBColor -rf; displayColor -rf; colorIndex -rf;")
        super(TestSelectionColorPreferences, self).tearDown()

    def setPref(self, name, r, g, b):
        '''Change a colour preference and render one frame.'''
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
        # Wireframe is the display style that uses polymeshDormant. Nothing is
        # selected, so only the every-prim colour is involved.
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