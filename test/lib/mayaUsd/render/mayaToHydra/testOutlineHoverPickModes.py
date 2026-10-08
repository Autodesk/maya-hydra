#
# Copyright 2026 Autodesk, Inc. All rights reserved.
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

# HYDRA-2456 : the outline hover highlight shows exactly what a click would select, so it follows
# the USD point instances pick mode (instancer, instance, or prototype), native instances included,
# and the USD selection kind. The hover helpers are those of testOutlineHover.py.

import os

import maya.cmds as cmds
import maya.OpenMaya as om
import maya.OpenMayaUI as omui

import fixturesUtils
import imageUtils
import mayaUtils
import mtohUtils
import testUtils
import ufe

from PySide6.QtCore import QEvent, QPoint, Qt
from PySide6.QtGui import QMouseEvent
from PySide6.QtWidgets import QApplication, QWidget
import shiboken6

# Render global enabling the outline mode's hover highlight, off by default.
HOVER_ATTR_NAME = "mayaHydraOutlineHoverHighlighting"
HOVER_ATTR = "defaultRenderGlobals.{}".format(HOVER_ATTR_NAME)

PICK_MODE_OPTION_VAR = "mayaUsd_PointInstancesPickMode"
SELECTION_KIND_OPTION_VAR = "mayaUsd_SelectionKind"

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

class TestOutlineHoverPickModes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 0.5

    def setUp(self):
        super(TestOutlineHoverPickModes, self).setUp()
        self._failures = []
        self._hoverEnabled = False
        self._savedOptionVars = {}

        if self.selectionHighlightMode() != mtohUtils.SELECTION_HIGHLIGHT_MODE_OUTLINE:
            self.skipTest("Hover highlighting only exists in the outline selection-highlight mode.")

    def tearDown(self):
        ufe.GlobalSelection.get().clear()

        if self._hoverEnabled:
            cmds.setAttr(HOVER_ATTR, False)
            cmds.mayaHydra(updateRenderGlobals=HOVER_ATTR_NAME)
            self._hoverEnabled = False

        for name, value in self._savedOptionVars.items():
            if value is None:
                cmds.optionVar(remove=name)
            else:
                cmds.optionVar(sv=(name, value))

        if self._failures:
            failureMessages = ["  - {}: {}".format(name, error) for name, error in self._failures]
            self.fail("Image comparison failures in {}:\n{}".format(
                self._testMethodName, "\n".join(failureMessages)))

        super(TestOutlineHoverPickModes, self).tearDown()

    def setOptionVar(self, name, value):
        if name not in self._savedOptionVars:
            self._savedOptionVars[name] = (
                cmds.optionVar(q=name) if cmds.optionVar(exists=name) else None)
        cmds.optionVar(sv=(name, value))

    def setPickMode(self, pickMode):
        self.setOptionVar(PICK_MODE_OPTION_VAR, pickMode)

    def compareSnapshot(self, referenceFilename):
        """Compare a snapshot, collecting failures so one bad frame does not hide the rest.

        Captures at TARGET_VIEWPORT_SIZE, not at the panel's actual size: the panel converges
        within _SIZE_CONVERGENCE_TOLERANCE, so its size can differ by a pixel between runs.
        """
        try:
            width, height = TARGET_VIEWPORT_SIZE

            refImagePath = self.resolveRefImage(referenceFilename, None)
            snapImagePath = os.path.join(self.getSnapshotDir(), referenceFilename)
            imageUtils.snapshot(snapImagePath, width=width, height=height)

            self.assertImagesClose(
                refImagePath, snapImagePath,
                self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        except Exception as e:
            self._failures.append((referenceFilename, str(e)))

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

    def loadUsdScene(self, sceneDir, sceneName, cameraTranslate, cameraRotate):
        self._forceDeterministicMainWindowSize()
        self.setHdStormRenderer()

        import usdUtils
        usdUtils.createStageFromFile(testUtils.getTestScene(sceneDir, sceneName + '.usda'))
        self._stagePathSegment = "|" + sceneName + "|" + sceneName + "Shape"
        # createStageFromFile() selects the new proxy shape, which would outline the whole stage.
        ufe.GlobalSelection.get().clear()
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.setAttr('persp.translate', *cameraTranslate, type='float3')
        cmds.setAttr('persp.rotate', *cameraRotate, type='float3')
        cmds.refresh(force=True)
        self.enableHover()

    def createItem(self, usdPath):
        return ufe.Hierarchy.createItem(
            ufe.PathString.path(self._stagePathSegment + "," + usdPath))

    def select(self, usdPath):
        sn = ufe.GlobalSelection.get()
        sn.clear()
        sn.append(self.createItem(usdPath))

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

    def _worldToViewportPixel(self, worldPos):
        """Viewport pixel of a world position (Python version of cpp/testUtils.cpp's
        getPrimMouseCoords())."""
        view = self._activeView()

        point = om.MPoint(worldPos[0], worldPos[1], worldPos[2])

        xUtil = om.MScriptUtil()
        xUtil.createFromInt(0)
        xPtr = xUtil.asShortPtr()
        yUtil = om.MScriptUtil()
        yUtil.createFromInt(0)
        yPtr = yUtil.asShortPtr()

        notClipped = view.worldToView(point, xPtr, yPtr)
        self.assertTrue(notClipped, "{} is clipped by the current camera view".format(worldPos))

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

    def hoverWorldPosition(self, worldPos):
        # The render override drops a mouse move to the pixel the cursor is already on, and resolves
        # the hover on mouse moves only. Moving over the background first makes hovering the same
        # position again, after a pick mode or selection kind change, resolve it with the new one.
        self.hoverEmptyBackground()
        self.hoverAt(self._worldToViewportPixel(worldPos))

    def hoverEmptyBackground(self):
        # A viewport corner, away from all prims.
        self.hoverAt(QPoint(5, 5))

    def test_PointInstancer(self):
        # One point instancer, two rows of 7 grey cubes of size 1 (z = 0 and z = -1.5, x = -4.5 to
        # 4.5 every 1.5). Each row uses the 7 prototypes once: instance 3 is at the origin,
        # instance 10, of the same prototype, behind it. The camera looks straight down, so
        # instances 0 to 6 are the bottom row of the image.
        self.loadUsdScene('testOutlineHoverPickModes', 'greyPointInstancer',
                          (0, 12, -0.75), (-90, 0, 0))
        instance3 = (0, 0, 0)

        # A click selects the instancer: all 14 cubes.
        self.setPickMode('PointInstancer')
        self.hoverWorldPosition(instance3)
        self.compareSnapshot("pi_hover_pointInstancer.png")

        # A click selects the instance: only the bottom-center cube.
        self.setPickMode('Instances')
        self.hoverWorldPosition(instance3)
        self.compareSnapshot("pi_hover_instances.png")

        # A click selects the prototype: the center cube of both rows.
        self.setPickMode('Prototypes')
        self.hoverWorldPosition(instance3)
        self.compareSnapshot("pi_hover_prototypes.png")

        # Instance 4 selected (lead color), and its neighbor, instance 3, hovered: two instances of
        # one instancer, each with its own color.
        self.setPickMode('Instances')
        self.hoverEmptyBackground()
        self.select("/Root/CubePointInstancer/4")
        self.hoverWorldPosition(instance3)
        self.compareSnapshot("pi_hover_instance3_selected4.png")

        # Back to background: the selection only.
        self.hoverEmptyBackground()
        self.compareSnapshot("pi_selected4_no_hover.png")

    def test_NestedNativeInstances(self):
        # Rows Outer_0, Outer_1, Outer_2 at z = 0, 4, 8, each the native instances Inner_0, Inner_1,
        # Inner_2 of a cube at x = 0, 3, 6. Orientation of testOutlineNativeInstances' camera
        # (setBasicCam(), aimed at the center), moved closer so the cubes fill the image.
        self.loadUsdScene('testUsdNativeInstances', 'nestedNativeInstances',
                          (11.4, 6.3, 12.4), (-30, 45, 0))
        inner1OfOuter1 = (3, 0, 4)

        # A click selects the outer native instance in both modes: the three cubes of the middle
        # row.
        self.setPickMode('PointInstancer')
        self.hoverWorldPosition(inner1OfOuter1)
        self.compareSnapshot("native_hover_outer1.png")

        self.setPickMode('Instances')
        self.hoverWorldPosition(inner1OfOuter1)
        self.compareSnapshot("native_hover_outer1.png")

        # A click selects the cube through both native instances: only the middle cube of the
        # middle row.
        self.setPickMode('Prototypes')
        self.hoverWorldPosition(inner1OfOuter1)
        self.compareSnapshot("native_hover_inner1OfOuter1.png")

        # The middle row selected (lead color), its middle cube hovered: one instance of the
        # selected rprims gets the hover color, the other two keep the lead color.
        self.hoverEmptyBackground()
        self.select("/Root/Outer_1")
        self.hoverWorldPosition(inner1OfOuter1)
        self.compareSnapshot("native_hover_inner1_selected_outer1.png")

    def test_SelectionKind(self):
        # A component model of two cubes (CubeA at x = -1.5, CubeB at x = 0), and a cube outside
        # any model (Other, at x = 1.5).
        self.loadUsdScene('testOutlineHoverPickModes', 'componentModel', (0, 0, 6), (0, 0, 0))
        cubeA = (-1.5, 0, 0)

        # No selection kind: a click selects the cube.
        self.setOptionVar(SELECTION_KIND_OPTION_VAR, "")
        self.hoverWorldPosition(cubeA)
        self.compareSnapshot("kind_hover_none.png")

        # Component kind: a click selects the model, both of its cubes.
        self.setOptionVar(SELECTION_KIND_OPTION_VAR, "component")
        self.hoverWorldPosition(cubeA)
        self.compareSnapshot("kind_hover_component.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
