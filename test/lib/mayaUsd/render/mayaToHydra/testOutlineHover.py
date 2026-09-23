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

import os

import maya.cmds as cmds
import maya.OpenMaya as om
import maya.OpenMayaUI as omui

import fixturesUtils
import imageUtils
import mayaUtils
import mtohUtils

from PySide6.QtCore import QEvent, QPoint, Qt
from PySide6.QtGui import QMouseEvent
from PySide6.QtWidgets import QApplication, QWidget
import shiboken6

# Render global enabling the outline mode's hover highlight, off by default.
HOVER_ATTR_NAME = "mayaHydraOutlineHoverHighlighting"
HOVER_ATTR = "defaultRenderGlobals.{}".format(HOVER_ATTR_NAME)

# The hover position is kept in device pixels and dropped if it falls outside the rendered
# viewport, so the panel size must match between hovering and capture, and be the same on every
# machine for the reference images to compare.
TARGET_VIEWPORT_SIZE = (400, 400)

# First guess for the main window size; _forceDeterministicMainWindowSize() then corrects it
# until the panel reaches TARGET_VIEWPORT_SIZE.
_INITIAL_MAIN_WINDOW_SIZE = (
    TARGET_VIEWPORT_SIZE[0] + 1200, TARGET_VIEWPORT_SIZE[1] + 700)

_MAX_SIZE_CONVERGENCE_ATTEMPTS = 8
_SIZE_CONVERGENCE_TOLERANCE = 1

class TestOutlineHover(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def compareSnapshot(self, referenceFilename):
        """Compare a snapshot, collecting failures so one bad frame does not hide the rest.

        Captures at the panel's actual size; see TARGET_VIEWPORT_SIZE.
        """
        try:
            widget = self._viewWidget(self._activeView())
            width, height = widget.width(), widget.height()

            refImagePath = self.resolveRefImage(referenceFilename, None)
            snapImagePath = os.path.join(self.getSnapshotDir(), referenceFilename)
            imageUtils.snapshot(snapImagePath, width=width, height=height)

            self.assertImagesClose(
                refImagePath, snapImagePath,
                self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        except Exception as e:
            self._failures.append((referenceFilename, str(e)))

    def setUp(self):
        super(TestOutlineHover, self).setUp()
        self._failures = []
        self._hoverEnabled = False

        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Hover highlighting only exists in the outline selection-highlight mode.")

    def tearDown(self):
        if self._hoverEnabled:
            cmds.setAttr(HOVER_ATTR, False)
            cmds.mayaHydra(updateRenderGlobals=HOVER_ATTR_NAME)
            self._hoverEnabled = False

        if self._failures:
            failureMessages = ["  - {}: {}".format(name, error) for name, error in self._failures]
            self.fail("Image comparison failures in {}:\n{}".format(
                self._testMethodName, "\n".join(failureMessages)))

        super(TestOutlineHover, self).tearDown()

    def enableHover(self):
        # The render override only re-reads the plug on 'mayaHydra -updateRenderGlobals'.
        cmds.setAttr(HOVER_ATTR, True)
        cmds.mayaHydra(updateRenderGlobals=HOVER_ATTR_NAME)
        cmds.refresh(force=True)
        self._hoverEnabled = True

    def _forceDeterministicMainWindowSize(self):
        mainWindowPtr = omui.MQtUtil.mainWindow()
        if not mainWindowPtr:
            return
        mainWindow = shiboken6.wrapInstance(int(mainWindowPtr), QWidget)

        def resizeAndSettle(size):
            mainWindow.resize(*size)
            QApplication.processEvents()
            cmds.refresh(force=True)

        # Correct by the remaining gap on each attempt: panel size need not track window size
        # linearly.
        size = list(_INITIAL_MAIN_WINDOW_SIZE)
        for _ in range(_MAX_SIZE_CONVERGENCE_ATTEMPTS):
            resizeAndSettle(size)
            panel = self._viewWidget(self._activeView())
            diffW = TARGET_VIEWPORT_SIZE[0] - panel.width()
            diffH = TARGET_VIEWPORT_SIZE[1] - panel.height()
            if abs(diffW) <= _SIZE_CONVERGENCE_TOLERANCE and abs(diffH) <= _SIZE_CONVERGENCE_TOLERANCE:
                return
            size[0] += diffW
            size[1] += diffH

        self.fail(
            "Could not converge the viewport panel to {}x{} after {} attempts (last size "
            "{}x{}).".format(
                TARGET_VIEWPORT_SIZE[0], TARGET_VIEWPORT_SIZE[1],
                _MAX_SIZE_CONVERGENCE_ATTEMPTS, panel.width(), panel.height()))

    def buildScene(self):
        self._forceDeterministicMainWindowSize()
        self.setHdStormRenderer()
        cmds.refresh(force=True)

        spheres = []
        for i in range(4):
            name = cmds.polySphere(radius=1.5, name="hoverSphere{}".format(i + 1))[0]
            # Staggered in depth with a small lateral offset, so each sphere partially overlaps
            # its neighbours on screen and nearer spheres occlude farther ones.
            cmds.move(i * 0.4, i * 0.15, -i * 2.5, name)
            spheres.append(name)
        cmds.viewFit(spheres)
        cmds.refresh(force=True)
        return spheres

    def _activeView(self):
        # Not self.activeEditor: the editor with focus is not reliably the model panel.
        panel = mayaUtils.activeModelPanel()
        view = omui.M3dView()
        omui.M3dView.getM3dViewFromModelPanel(panel, view)
        return view

    def _viewWidget(self, view):
        # view.widget() is the render surface the hover event filter is installed on, unlike
        # MQtUtil.findControl(panelName), which returns the outer panel frame.
        return shiboken6.wrapInstance(int(view.widget()), QWidget)

    def _worldToViewportPixel(self, objName):
        """Viewport pixel of the object's world translation (Python version of
        cpp/testUtils.cpp's getPrimMouseCoords())."""
        view = self._activeView()

        pos = cmds.xform(objName, query=True, worldSpace=True, translation=True)
        point = om.MPoint(pos[0], pos[1], pos[2])

        xUtil = om.MScriptUtil()
        xUtil.createFromInt(0)
        xPtr = xUtil.asShortPtr()
        yUtil = om.MScriptUtil()
        yUtil.createFromInt(0)
        yPtr = yUtil.asShortPtr()

        notClipped = view.worldToView(point, xPtr, yPtr)
        self.assertTrue(notClipped, "{} is clipped by the current camera view".format(objName))

        x = om.MScriptUtil.getShort(xPtr)
        y = om.MScriptUtil.getShort(yPtr)
        # Qt and M3dView use opposite Y-coordinates.
        return QPoint(x, view.portHeight() - y)

    def hoverAt(self, localPos):
        widget = self._viewWidget(self._activeView())
        globalPos = widget.mapToGlobal(localPos)
        event = QMouseEvent(
            QEvent.Type.MouseMove, localPos, globalPos,
            Qt.MouseButton.NoButton, Qt.MouseButtons(Qt.MouseButton.NoButton),
            Qt.KeyboardModifiers())
        QApplication.sendEvent(widget, event)
        cmds.refresh(force=True)

    def hoverObject(self, objName):
        self.hoverAt(self._worldToViewportPixel(objName))

    def hoverEmptyBackground(self):
        # A viewport corner, away from all spheres.
        self.hoverAt(QPoint(5, 5))

    def test_HoverDisabled(self):
        """Negative control: with hover highlighting at its default (off), hovering draws
        nothing, with or without a selection."""
        s1, s2, s3, s4 = self.buildScene()
        cmds.select(clear=True)

        self.hoverObject(s4)
        self.compareSnapshot("hover_disabled_no_selection_no_cue.png")

        cmds.select(s2, add=True)
        self.hoverObject(s4)
        self.compareSnapshot("hover_disabled_with_selection_no_extra_cue.png")

    def test_HoverForwardPassAndOcclusion(self):
        """Select farthest-to-nearest, hovering each sphere before adding it. Covers hover alone,
        hover over lead and non-lead selections, and outlines occluded by nearer unselected
        spheres. Ends by hovering each already-selected sphere."""
        s1, s2, s3, s4 = self.buildScene()
        cmds.select(clear=True)
        self.enableHover()

        self.hoverObject(s4)
        self.compareSnapshot("hover_s4_alone.png")

        self.hoverEmptyBackground()
        cmds.select(s4)
        self.compareSnapshot("select_s4_lead.png")

        self.hoverObject(s3)
        self.compareSnapshot("hover_s3_over_s4_lead.png")

        self.hoverEmptyBackground()
        cmds.select(s3, add=True)
        self.compareSnapshot("select_s3_lead_s4_nonlead.png")

        self.hoverObject(s2)
        self.compareSnapshot("hover_s2_over_s3lead_s4nonlead.png")

        self.hoverEmptyBackground()
        cmds.select(s2, add=True)
        self.compareSnapshot("select_s2_lead_s3s4_nonlead.png")

        self.hoverObject(s1)
        self.compareSnapshot("hover_s1_over_s2lead_s3s4nonlead.png")

        self.hoverEmptyBackground()
        cmds.select(s1, add=True)
        self.compareSnapshot("select_s1_lead_allNonlead.png")

        # Hover on lead vs non-lead, with the selection unchanged.
        self.hoverObject(s1)
        self.compareSnapshot("hover_on_lead_s1.png")

        self.hoverObject(s2)
        self.compareSnapshot("hover_on_nonlead_s2.png")

        self.hoverObject(s3)
        self.compareSnapshot("hover_on_nonlead_s3.png")

        self.hoverObject(s4)
        self.compareSnapshot("hover_on_nonlead_s4.png")

    def test_HoverReverseOcclusion(self):
        """Select nearest-to-farthest and hover the farthest, unselected sphere each time, so a
        nearer selected sphere occludes the hovered one (a case the forward pass cannot produce).
        HVT gives selected outlines priority over unselected ones regardless of depth, so the images
        pin down how the hover highlight is drawn at the overlap."""
        s1, s2, s3, s4 = self.buildScene()
        cmds.select(clear=True)
        self.enableHover()

        self.hoverEmptyBackground()
        cmds.select(s1)
        self.compareSnapshot("reverse_select_s1_lead_only.png")

        self.hoverObject(s4)
        self.compareSnapshot("reverse_hover_s4_behind_selected_s1.png")

        self.hoverEmptyBackground()
        cmds.select(s2, add=True)
        self.compareSnapshot("reverse_select_s2_lead_s1_nonlead.png")

        self.hoverObject(s4)
        self.compareSnapshot("reverse_hover_s4_behind_selected_s1s2.png")

        self.hoverEmptyBackground()
        cmds.select(s3, add=True)
        self.compareSnapshot("reverse_select_s3_lead_s1s2_nonlead.png")

        self.hoverObject(s4)
        self.compareSnapshot("reverse_hover_s4_behind_selected_s1s2s3.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
