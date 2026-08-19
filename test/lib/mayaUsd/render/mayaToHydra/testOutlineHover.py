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

# Script-only render global toggling outline mode's hover cue, off by default. See
# lib/mayaHydra/mayaPlugin/tokens.h and renderGlobals.cpp.
HOVER_ATTR_NAME = "mayaHydraOutlineHoverHighlighting"
HOVER_ATTR = "defaultRenderGlobals.{}".format(HOVER_ATTR_NAME)

# Hover state is cached as absolute device pixel coordinates (renderOverride.cpp's
# _SetHoverPosition()/HoverState), and _ResolveHoverPath() bounds-checks them against whatever
# render's viewport dimensions are current. So the viewport panel's pixel size has to match between
# firing and capture, and has to be reproducible across machines for the reference images to compare
# cleanly -- neither holds for the panel's ambient size, hence forcing it to a fixed target.
TARGET_VIEWPORT_SIZE = (400, 400)

# First guess for the main window size; how much of it the panel actually gets depends on the
# machine/theme/DPI/layout, so _forceDeterministicMainWindowSize() corrects toward the target
# empirically rather than assuming this reaches it directly.
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
        """Compare snapshot and collect failures instead of stopping on first failure, so one bad
        frame in this long sequence does not hide failures in the rest of it.

        Captures at the viewport panel's actual size (forced to TARGET_VIEWPORT_SIZE by
        buildScene(), see the comment there) instead of a capture size independent of it -- a
        mismatch pushes hover's cached device-pixel coordinates out of bounds, silently dropping the
        hover cue rather than rescaling it.
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

        # Hover only exists in outline mode: nothing to test under legacy.
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
        # Setting the plug alone is not enough: the render override only re-reads the globals when
        # 'mayaHydra -updateRenderGlobals' is called (same caveat as the selection-highlight mode
        # enum, see mtohUtils.applySelectionHighlightMode()).
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

        # Correct by whatever gap remains each attempt, rather than assuming a single fixed offset
        # between window size and panel size: that relationship need not be linear (e.g. a
        # fixed-width side panel can leave the panel's width unmoved by the window's width).
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
            # Staggered along the view axis and offset laterally, so each sphere partially overlaps
            # its neighbours on screen once framed -- nearer, unselected spheres act as occluders for
            # farther, hovered/selected ones. Lateral drift is kept small relative to the depth step
            # so viewFit's zoom-to-fit-all-4 doesn't shrink the chain apart -- s1 (dead center, no
            # lateral offset) needs to still overlap s2 after framing accounts for s4's total offset.
            cmds.move(i * 0.4, i * 0.15, -i * 2.5, name)
            spheres.append(name)
        cmds.viewFit(spheres)
        cmds.refresh(force=True)
        return spheres

    def _activeView(self):
        # self.activeEditor (from cmds.playblast(activeEditor=1), set by setHdStormRenderer()) is
        # whatever editor currently has UI focus, which is not reliably the 3D viewport --
        # mayaUtils.activeModelPanel() specifically finds the model panel.
        panel = mayaUtils.activeModelPanel()
        view = omui.M3dView()
        omui.M3dView.getM3dViewFromModelPanel(panel, view)
        return view

    def _viewWidget(self, view):
        # view.widget() -- the exact widget HoverEventFilter is installed on and calls
        # setMouseTracking(true) on (renderOverride.cpp's _InstallHoverEventFilter()) -- is not the
        # same widget as MQtUtil.findControl(panelName), which returns the outer panel frame (full of
        # toolbar buttons); the real render surface is a QStackedWidget nested inside it. view.widget()
        # returns a raw SWIG QWidget* that needs int()-converting before shiboken6 can wrap it.
        return shiboken6.wrapInstance(int(view.widget()), QWidget)

    def _worldToViewportPixel(self, objName):
        """Port of cpp/testUtils.cpp's getPrimMouseCoords() to Python, using the object's world
        translation directly instead of a scene-index prim's xform data source."""
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
        """Negative control: with mayaHydraOutlineHoverHighlighting left at its default (False),
        hovering does nothing -- neither against an empty selection nor on top of an existing one.
        Guards against a broken "off" state silently producing a false pass in the other two tests."""
        s1, s2, s3, s4 = self.buildScene()
        cmds.select(clear=True)

        self.hoverObject(s4)
        self.compareSnapshot("hover_disabled_no_selection_no_cue.png")

        cmds.select(s2, add=True)
        self.hoverObject(s4)
        self.compareSnapshot("hover_disabled_with_selection_no_extra_cue.png")

    def test_HoverForwardPassAndOcclusion(self):
        """Select farthest-to-nearest, hovering the next sphere before adding it each time. Every
        sphere added is nearer than everything already selected, and the sphere being hovered right
        before its own turn is always the nearer, still-unselected one relative to what's already
        selected -- so this demonstrates the outline surviving occlusion by nearer, UNSELECTED
        spheres. s4 (furthest) hovered alone is also occluded on screen by the nearer, unselected
        s1-s3, so the hover cue should already be unbroken across that overlap in the first step."""
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

        # Hover each already-selected sphere in turn without touching the selection, to isolate the
        # hover-on-lead vs hover-on-non-lead cues.
        self.hoverObject(s1)
        self.compareSnapshot("hover_on_lead_s1.png")

        self.hoverObject(s2)
        self.compareSnapshot("hover_on_nonlead_s2.png")

        self.hoverObject(s3)
        self.compareSnapshot("hover_on_nonlead_s3.png")

        self.hoverObject(s4)
        self.compareSnapshot("hover_on_nonlead_s4.png")

    def test_HoverReverseOcclusion(self):
        """Select nearest-to-farthest -- the mirror image of the forward pass. Every sphere selected
        here is nearer than everything selected after it, and each step hovers s4 (farthest, still
        unselected) -- so a nearer SELECTED sphere now occludes the farther, unselected, hovered one
        on screen: the reverse-occlusion case the forward pass structurally can't produce (it always
        selects the farthest sphere first, so a nearer sphere is never selected while a farther one
        remains unselected). Per lib/hydra-viewport-toolbox/docs/outline.md:42-77, cross-layer
        priority is Base (selected) > Default (unselected) unconditionally -- not depth-based, and
        not covered by the same-layer hover-protection rule -- so this could plausibly suppress the
        hover cue at the overlap where the forward pass's arrangement does not. Whichever result
        actually happens is a useful fact worth pinning down, not a presumed pass/fail."""
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
