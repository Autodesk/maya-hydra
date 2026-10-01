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

import os
import shutil

import fixturesUtils
import maya.cmds as cmds
import maya.mel as mel
import mtohUtils
from maya.api import OpenMaya
from PySide6.QtCore import QEventLoop, QTimer
from PySide6.QtWidgets import QApplication, QMessageBox

# It can take a while until Maya receives the first progress message. Wait for 60 seconds.
_TIMEOUT_MS = 60000
_SCENES_DIR = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "scenes", "basic")


def _scenePath(sceneFileName):
    return os.path.join(_SCENES_DIR, sceneFileName)


class TestCancelBatchRender(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _initializeStandalone = False
    _setHdStormRenderer = False
    _requiredPlugins = ["mtoa"]

    def test_CancelBatchRender(self):
        # Copy the scene to temporary directory pointing to MAYA_APP_DIR
        # to avoid polluting the source directory.
        sceneFileName = "cancelBatchRender.ma"
        tempAppDir = os.environ.get("MAYA_APP_DIR")
        if tempAppDir:
            destDir = os.path.join(tempAppDir, "projects", "default", "scenes")
            if not os.path.isdir(destDir):
                os.makedirs(destDir)
            baseName = os.path.splitext(sceneFileName)[0]
            for extension in (".ma", ".usda"):
                fileName = baseName + extension
                shutil.copy(_scenePath(fileName), os.path.join(destDir, fileName))
            scenePath = os.path.join(destDir, sceneFileName)
        else:
            scenePath = _scenePath(sceneFileName)

        cmds.file(scenePath, open=True, force=True)

        # We want to collect all the messages from the batch render and check
        # whether it was cancelled successfully or failed due to other reasons.
        messages = []
        state = {
            "cancelScheduled": False,
            "error": None,
            "timedOut": False,
        }
        eventLoop = QEventLoop()

        def acceptCancelRenderDialog():
            for widget in QApplication.topLevelWidgets():
                if not isinstance(widget, QMessageBox) or not widget.isVisible():
                    continue
                for button in widget.buttons():
                    if widget.buttonRole(button) == QMessageBox.ButtonRole.YesRole:
                        button.click()
                        return

        def cancelBatchRender():
            try:
                # Interactive Maya asks for confirmation before cancelling. Queue the
                # response first.
                QTimer.singleShot(0, acceptCancelRenderDialog)
                cmds.batchRender()
            except Exception as exception:
                state["error"] = exception
            finally:
                eventLoop.quit()

        def commandOutputCallback(message, _messageType, _clientData):
            messages.append(message)

            # When a child renderer registers its PID to Maya using the FRAME_STARTED (-1111)
            # magic number in sendRenderProgressInfo(), Maya outputs
            # "Percentage of rendering done: 0", which means it's successfully registered
            # and can be cancelled by Maya using its PID. Catch this in the message and
            # schedule cancel batch render.
            if ("Percentage of rendering done: 0" in message
                    and not state["cancelScheduled"]):
                state["cancelScheduled"] = True
                # Schedule cancellation outside the output callback to avoid re-entering
                # Maya's command engine while it is handling the status message.
                QTimer.singleShot(0, cancelBatchRender)

        def timeout():
            state["timedOut"] = True
            eventLoop.quit()

        callbackId = OpenMaya.MCommandMessage.addCommandOutputCallback(commandOutputCallback)
        timeoutTimer = QTimer()
        timeoutTimer.setSingleShot(True)
        timeoutTimer.timeout.connect(timeout)

        try:
            # Launch the batch render via MEL command.
            mel.eval("mayaBatchRender()")
            # Because mayaBatchRender() returns immediately while the child process renders,
            # we need to place an event loop to collect the output messages.
            # It will terminate when batch render is cancelled, or when it times out.
            timeoutTimer.start(_TIMEOUT_MS)
            eventLoop.exec()
        finally:
            timeoutTimer.stop()
            OpenMaya.MMessage.removeCallback(callbackId)

        output = "\n".join(messages)
        if state["error"] is not None:
            raise state["error"]
        self.assertFalse(
            state["timedOut"],
            "Timed out waiting for Maya's 0% FRAME_STARTED notification")
        self.assertIn(
            "Render Cancelled",   # Succeeded.
            output,
            "Maya did not report batch-render cancellation:\n" + output)
        self.assertNotIn(
            "previous batch render has not started",
            output,
            "Maya tried to cancel before registering the renderer PID:\n" + output)


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
