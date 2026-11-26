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

# HYDRA-2704 : the viewports of one renderer share its frame passes, which hold the camera and
# framing of the last viewport drawn. A pick in another viewport must still use that viewport's
# own camera and framing. Before the fix, with Flash, a click in a viewport other than the last one
# drawn selected nothing.
# The USD stage is created while both viewports exist: each view adds it to the render override's
# shared data producer merging scene index, which used to merge it twice ("Observer is already
# registered").

import maya.cmds as cmds
import maya.mel as mel
import maya.OpenMayaUI as omui

import fixturesUtils
import mtohUtils
import rendererUtils
import ufe

from testUtils import PluginLoaded

class TestPickingSharedFramePasses(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    MAYA_CUBE = "pickMayaCube"
    USD_CUBE = "pickUsdCube"
    # Off the origin and apart, so that each cube is drawn at different pixels through the top
    # and perspective cameras.
    MAYA_CUBE_POSITION = (-3, 0, 2)
    USD_CUBE_POSITION = (3, 0, -2)

    def tearDown(self):
        ufe.GlobalSelection.get().clear()
        super(TestPickingSharedFramePasses, self).tearDown()

    def setUpTwoViewports(self):
        '''Two side-by-side viewports on the test renderer: the first through the orthographic top
        camera, the second through the perspective camera.'''
        mel.eval("TwoSideBySideViewArrangement")
        panels = [p for p in cmds.getPanel(visiblePanels=True)
                  if cmds.getPanel(typeOf=p) == "modelPanel"]
        self.assertGreaterEqual(len(panels), 2, "Expected 2 visible model panels")
        self._panels = panels[:2]

        override = rendererUtils.getTestRendererOverrideName()
        for panel, camera in zip(self._panels, ("top", "persp")):
            cmds.modelEditor(panel, edit=True, camera=camera)
            cmds.modelEditor(panel, edit=True, rendererOverrideName=override)
        cmds.refresh(force=True)
        for panel in self._panels:
            self.assertEqual(
                cmds.modelEditor(panel, query=True, rendererOverrideName=True), override)

    def createScene(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib
        from pxr import UsdGeom

        cmds.polyCube(name=self.MAYA_CUBE)
        cmds.move(*self.MAYA_CUBE_POSITION)

        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        xform = UsdGeom.Xform.Define(stage, "/" + self.USD_CUBE + "Xform")
        xform.AddTranslateOp().Set(value=self.USD_CUBE_POSITION)
        UsdGeom.Cube.Define(stage, str(xform.GetPath()) + "/" + self.USD_CUBE)

        cmds.viewFit("top", allObjects=True)
        cmds.viewFit("persp", allObjects=True)
        cmds.select(clear=True)
        cmds.refresh(force=True)

    def drawLast(self, panel):
        '''Draw every viewport, then this one alone, so that it is the last one drawn.'''
        cmds.refresh(force=True)
        view = omui.M3dView()
        omui.M3dView.getM3dViewFromModelPanel(panel, view)
        view.refresh(False, True)

    def pickWithOtherViewportDrawnLast(self):
        with PluginLoaded('mayaHydraCppTests'):
            first, second = self._panels
            for pickedPanel, lastDrawnPanel in ((first, second), (second, first)):
                for objectName in (self.MAYA_CUBE, self.USD_CUBE):
                    cmds.select(clear=True)
                    ufe.GlobalSelection.get().clear()
                    # The click goes to the active viewport.
                    cmds.setFocus(pickedPanel)
                    self.drawLast(lastDrawnPanel)
                    # Clicks the prim in the active viewport, then checks its Hydra selection.
                    cmds.mayaHydraCppTest(objectName, "mesh", f="TestPicking.pickObject")

    def test_PickWithOtherViewportDrawnLast(self):
        self.setUpTwoViewports()
        self.createScene()
        self.pickWithOtherViewportDrawnLast()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
