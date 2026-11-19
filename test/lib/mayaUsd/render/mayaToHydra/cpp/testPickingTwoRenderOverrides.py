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

# HYDRA-2726 : with Storm and Flash live in two viewports, tearing Flash down must not break
# picking in Storm. The pick context, pick handlers and path mappers are process-wide, and used
# to belong to a single override. Before the fix:
# - Storm first: hovering or clicking a USD prim crashed (null pick context).
# - Flash first: neither Maya objects nor USD prims could be picked (no pick handler).

import maya.cmds as cmds
import maya.mel as mel

import fixturesUtils
import mayaUtils
import mtohUtils
import rendererUtils
import ufe

from testUtils import PluginLoaded

STORM = rendererUtils.getRendererOverrideName(rendererUtils.TEST_RENDERER_STORM)
FLASH = rendererUtils.getRendererOverrideName(rendererUtils.TEST_RENDERER_FLASH)

# Render global enabling the outline mode's hover highlight, off by default.
HOVER_ATTR_NAME = "mayaHydraOutlineHoverHighlighting"
HOVER_ATTR = "defaultRenderGlobals.{}".format(HOVER_ATTR_NAME)

USE_HYDRA_OPTION_VAR = "mayaUseHydra"

class TestPickingTwoRenderOverrides(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    MAYA_CUBE = "pickMayaCube"
    USD_CUBE = "pickUsdCube"
    MAYA_CUBE_POSITION = (-2, 0, 0)
    USD_CUBE_POSITION = (2, 0, 0)

    def setUp(self):
        self._savedUseHydra = (
            cmds.optionVar(query=USE_HYDRA_OPTION_VAR)
            if cmds.optionVar(exists=USE_HYDRA_OPTION_VAR) else None)
        self._hoverEnabled = False

        super(TestPickingTwoRenderOverrides, self).setUp()

        overrides = cmds.modelEditor(
            mayaUtils.activeModelPanel(), query=True, rendererOverrideList=True) or []
        for override in (STORM, FLASH):
            if override not in overrides:
                self.skipTest("Needs both Storm and Flash, {} is not registered.".format(override))

    def tearDown(self):
        ufe.GlobalSelection.get().clear()

        # Normally already done by pickInEachViewport().
        self.disableHover()

        if self._savedUseHydra is None:
            cmds.optionVar(remove=USE_HYDRA_OPTION_VAR)
        else:
            cmds.optionVar(intValue=(USE_HYDRA_OPTION_VAR, self._savedUseHydra))

        super(TestPickingTwoRenderOverrides, self).tearDown()

    def setUpTwoViewports(self):
        '''Two side-by-side viewports through the perspective camera, both on VP2, so that
        neither Hydra render override is initialized.'''
        mel.eval("TwoSideBySideViewArrangement")
        panels = [p for p in cmds.getPanel(visiblePanels=True)
                  if cmds.getPanel(typeOf=p) == "modelPanel"]
        self.assertGreaterEqual(len(panels), 2, "Expected 2 visible model panels")
        self._panels = panels[:2]

        # Under Maya's Use Hydra preference, VP2 is refused: turn it off to reach VP2.
        # applyUseHydraToModelPanels does the 'ogs -reset' VP2 needs after Hydra.
        if mtohUtils.mayaHasHydraMode():
            cmds.optionVar(intValue=(USE_HYDRA_OPTION_VAR, 0))
        for panel in self._panels:
            cmds.modelEditor(panel, edit=True, rendererOverrideName="")
            cmds.modelEditor(panel, edit=True, camera="persp")
        if mtohUtils.mayaHasHydraMode():
            mel.eval("applyUseHydraToModelPanels")
        cmds.refresh(force=True)

    def createScene(self):
        cmds.polyCube(name=self.MAYA_CUBE)
        cmds.move(*self.MAYA_CUBE_POSITION)

        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib
        from pxr import UsdGeom
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        xform = UsdGeom.Xform.Define(stage, "/" + self.USD_CUBE + "Xform")
        xform.AddTranslateOp().Set(value=self.USD_CUBE_POSITION)
        UsdGeom.Cube.Define(stage, str(xform.GetPath()) + "/" + self.USD_CUBE)

        self.setBasicCam(10)
        cmds.select(clear=True)

    def initializeOverrides(self, first, second):
        '''Initialize the first override in both viewports, then the second override in the second
        viewport, so that they initialize in a known order.'''
        if mtohUtils.mayaHasHydraMode():
            cmds.optionVar(intValue=(USE_HYDRA_OPTION_VAR, 1))
        for panel in self._panels:
            cmds.modelEditor(panel, edit=True, rendererOverrideName=first)
        if mtohUtils.mayaHasHydraMode():
            # Panels are all on Hydra: this only does the 'ogs -reset' a Use Hydra change needs.
            mel.eval("applyUseHydraToModelPanels")
        cmds.refresh(force=True)

        cmds.modelEditor(self._panels[1], edit=True, rendererOverrideName=second)
        cmds.refresh(force=True)

    def tearDownFlash(self):
        '''Switch the Flash viewports to Storm: Flash loses its last viewport and clears its Hydra
        resources, Storm keeps drawing both viewports.'''
        for panel in self._panels:
            if cmds.modelEditor(panel, query=True, rendererOverrideName=True) == FLASH:
                cmds.modelEditor(panel, edit=True, rendererOverrideName=STORM)
        cmds.refresh(force=True)
        for panel in self._panels:
            self.assertEqual(
                cmds.modelEditor(panel, query=True, rendererOverrideName=True), STORM)

    def disableHover(self):
        # Must run before a File New (PluginLoaded's exit does one), which drops this dynamic
        # attribute.
        if self._hoverEnabled and cmds.objExists(HOVER_ATTR):
            cmds.setAttr(HOVER_ATTR, False)
            cmds.mayaHydra(updateRenderGlobals=HOVER_ATTR_NAME)
        self._hoverEnabled = False

    def hoverUsdCube(self, panel):
        '''Hover the USD cube, which resolves it through the pick handlers like a click. Only in
        the outline mode, where hover highlighting exists, and with Qt for Python.'''
        if (self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE
                or not cmds.objExists(HOVER_ATTR)):
            return
        try:
            import maya.OpenMaya as om
            import maya.OpenMayaUI as omui
            from PySide6.QtCore import QEvent, QPoint, Qt
            from PySide6.QtGui import QMouseEvent
            from PySide6.QtWidgets import QApplication, QWidget
            import shiboken6
        except ImportError:
            return

        if not self._hoverEnabled:
            # The render override only re-reads the plug on 'mayaHydra -updateRenderGlobals'.
            cmds.setAttr(HOVER_ATTR, True)
            cmds.mayaHydra(updateRenderGlobals=HOVER_ATTR_NAME)
            cmds.refresh(force=True)
            self._hoverEnabled = True

        view = omui.M3dView()
        omui.M3dView.getM3dViewFromModelPanel(panel, view)
        # The render surface the hover event filter is installed on.
        widget = shiboken6.wrapInstance(int(view.widget()), QWidget)

        xUtil = om.MScriptUtil()
        xUtil.createFromInt(0)
        xPtr = xUtil.asShortPtr()
        yUtil = om.MScriptUtil()
        yUtil.createFromInt(0)
        yPtr = yUtil.asShortPtr()
        self.assertTrue(view.worldToView(om.MPoint(*self.USD_CUBE_POSITION), xPtr, yPtr))
        # Qt and M3dView use opposite Y-coordinates.
        cubePixel = QPoint(om.MScriptUtil.getShort(xPtr),
                           view.portHeight() - om.MScriptUtil.getShort(yPtr))

        # Over the background first: a move to the pixel the cursor is already on is dropped.
        for localPos in (QPoint(5, 5), cubePixel):
            event = QMouseEvent(
                QEvent.Type.MouseMove, localPos, widget.mapToGlobal(localPos),
                Qt.MouseButton.NoButton, Qt.MouseButtons(Qt.MouseButton.NoButton),
                Qt.KeyboardModifiers())
            QApplication.sendEvent(widget, event)
            cmds.refresh(force=True)

    def pickInEachViewport(self):
        with PluginLoaded('mayaHydraCppTests'):
            try:
                for panel in self._panels:
                    cmds.setFocus(panel)
                    cmds.refresh(force=True)

                    self.hoverUsdCube(panel)

                    # Clicks the prim in the active viewport, then checks its Hydra selection.
                    for objectName in (self.MAYA_CUBE, self.USD_CUBE):
                        cmds.select(clear=True)
                        ufe.GlobalSelection.get().clear()
                        cmds.refresh(force=True)
                        cmds.mayaHydraCppTest(objectName, "mesh", f="TestPicking.pickObject")
            finally:
                # Before PluginLoaded's File New removes the attribute.
                self.disableHover()

    def test_StormThenFlash(self):
        self.setUpTwoViewports()
        self.createScene()
        self.initializeOverrides(STORM, FLASH)
        self.tearDownFlash()
        self.pickInEachViewport()

    def test_FlashThenStorm(self):
        self.setUpTwoViewports()
        self.createScene()
        self.initializeOverrides(FLASH, STORM)
        self.tearDownFlash()
        self.pickInEachViewport()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
