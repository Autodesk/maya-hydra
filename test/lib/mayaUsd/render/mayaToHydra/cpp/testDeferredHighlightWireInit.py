# Copyright 2025 Autodesk
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
import maya.utils

import fixturesUtils
import mtohUtils

from testUtils import PluginLoaded

class TestDeferredHighlightWireInit(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_DeferredHighlightWireInit(self):
        self.setHdStormRenderer()
        panel = self.activeEditor

        # Shaded only -- no wireframe-on-shaded, no shade-active-only -- so
        # viewportDrawsWireframes() is false and a selected shape's wireframe
        # render item is skipped rather than translated.
        cmds.modelEditor(panel, edit=True, displayAppearance='smoothShaded',
                         wireframeOnShaded=False, activeOnly=False)
        cmds.refresh(force=True)

        # Off the origin, so a zero or uninitialized transform is distinguishable
        # from the correct one.
        cube = cmds.polyCube(w=2, h=2, d=2)[0]
        cmds.move(10, 5, -3, cube, absolute=True)
        cmds.select(cube, replace=True)
        shape = cmds.listRelatives(cube, shapes=True, fullPath=True)[0]

        # Drop every render-item adapter and re-send while the shape is selected:
        # this is what makes the DormantPolyWire reach the scene index for the
        # first time in the state that takes the skip. Without it the adapter may
        # already exist from a dormant frame and the deferred path is never
        # exercised -- selecting an already-translated object and then switching
        # display style does NOT reproduce the defect.
        cmds.ogs(reset=True)
        maya.utils.processIdleEvents()
        cmds.refresh(force=True)

        # Now make the panel draw wireframes. This is the transition that sets
        # reconsiderSkippedHighlightWires, so the adapter is created from a delta
        # that carries only visibility bits.
        cmds.modelEditor(panel, edit=True, displayAppearance='wireframe')
        cmds.refresh(force=True)

        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                shape, f="DeferredHighlightWireInit.testDeferredTransform")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())