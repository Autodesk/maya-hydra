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

import logging
import maya.cmds as cmds
import maya.mel as mel

_log = logging.getLogger(__name__)


def _unlock_and_enable_animation():
    mel.eval("removeRenderLayerAdjustmentAndUnlock defaultRenderGlobals.animation")
    cmds.setAttr("defaultRenderGlobals.animation", 1)


def setStartFrame(frame):
    """Enable animation mode and set defaultRenderGlobals.startFrame.

    Mirrors the Arnold native -s flag: enables animation mode and removes
    render-layer overrides so the command-line value is not masked.

    Raises RuntimeError if the frame is invalid (non-numeric) or if it
    exceeds the current end frame."""
    try:
        frame_val = float(frame)
    except (TypeError, ValueError):
        raise RuntimeError(
            "Start frame must be numeric, got: %s" % frame)

    # Validate against current end frame to catch obvious user errors.
    # mayaBatchRenderProcedure iterates from start to end, so start > end
    # would silently render 0 frames.
    end_frame = cmds.getAttr("defaultRenderGlobals.endFrame")
    if frame_val > end_frame:
        raise RuntimeError(
            "Start frame (%s) cannot exceed end frame (%s)."
            % (frame_val, end_frame))

    _unlock_and_enable_animation()
    mel.eval("removeRenderLayerAdjustmentAndUnlock defaultRenderGlobals.startFrame")
    cmds.setAttr("defaultRenderGlobals.startFrame", frame_val)
    _log.info("Set start frame to %.4g", frame_val)


def setEndFrame(frame):
    """Enable animation mode and set defaultRenderGlobals.endFrame.

    Mirrors the Arnold native -e flag: enables animation mode and removes
    render-layer overrides so the command-line value is not masked.

    Raises RuntimeError if the frame is invalid (non-numeric) or if it
    is below the current start frame."""
    try:
        frame_val = float(frame)
    except (TypeError, ValueError):
        raise RuntimeError(
            "End frame must be numeric, got: %s" % frame)

    # Validate against current start frame to catch obvious user errors.
    # mayaBatchRenderProcedure iterates from start to end, so start > end
    # would silently render 0 frames.
    start_frame = cmds.getAttr("defaultRenderGlobals.startFrame")
    if frame_val < start_frame:
        raise RuntimeError(
            "End frame (%s) cannot be less than start frame (%s)."
            % (frame_val, start_frame))

    _unlock_and_enable_animation()
    mel.eval("removeRenderLayerAdjustmentAndUnlock defaultRenderGlobals.endFrame")
    cmds.setAttr("defaultRenderGlobals.endFrame", frame_val)
    _log.info("Set end frame to %.4g", frame_val)
