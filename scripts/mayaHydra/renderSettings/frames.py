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
import re

from pxr import Gf, Sdf, Vt

from .utils import getRenderSettingsPrim

_log = logging.getLogger(__name__)

_FRAMES_ATTR = "adsk:frames"
_STEP_ATTR = "adsk:step"

# A single frame, or a hyphen-separated pair of frames.  Greedy matching binds
# a leading sign to its number, so "-10--5" and "-10 - -5" both give (-10, -5).
_NUM = r"[-+]?(?:\d+\.?\d*|\.\d+)"
_ITEM_RE = re.compile(r"^(%s)(?:\s*-\s*(%s))?$" % (_NUM, _NUM))


def _parseFrames(frames):
    """Return a list of (start, end) float pairs from a frame list string.

    The string is a comma-separated list of items, each item either a single
    frame f, expanded into the pair (f, f), or a hyphen-separated pair of
    frames.  Frames may be negative or zero.

    Raises RuntimeError if the string is malformed, if a pair ends before it
    starts, or if an item does not start strictly after the preceding item."""
    if not isinstance(frames, str):
        raise RuntimeError(
            "Frames must be a string, got: %s" % frames)

    if not frames.strip():
        raise RuntimeError("Frames must not be empty.")

    pairs = []
    prevEnd = None
    for item in frames.split(","):
        item = item.strip()
        match = _ITEM_RE.match(item)
        if not match:
            raise RuntimeError("Invalid frame item: %s" % item)

        start = float(match.group(1))
        end = start if match.group(2) is None else float(match.group(2))

        if end < start:
            raise RuntimeError(
                "Frame range end (%s) cannot be less than start (%s)."
                % (end, start))

        if prevEnd is not None and start <= prevEnd:
            raise RuntimeError(
                "Frame (%s) must be greater than preceding frame (%s)."
                % (start, prevEnd))

        pairs.append((start, end))
        prevEnd = end

    return pairs


def _getFramesPair(prim):
    """Return (start, end) from the first element of adsk:frames, or None."""
    attr = prim.GetAttribute(_FRAMES_ATTR)
    if attr and attr.HasAuthoredValue():
        frames = attr.Get()
        if frames is not None and len(frames) > 0:
            return (frames[0][0], frames[0][1])
    return None


def _setFramesArray(prim, pairs):
    """Write a double2[] of (start, end) pairs to adsk:frames on the prim."""
    attr = prim.GetAttribute(_FRAMES_ATTR)
    if not attr:
        attr = prim.CreateAttribute(
            _FRAMES_ATTR, Sdf.ValueTypeNames.Double2Array, custom=True)
    attr.Set(Vt.Vec2dArray([Gf.Vec2d(start, end) for start, end in pairs]))


def _setFramesPair(prim, start, end):
    """Write a single-element double2[] to adsk:frames on the prim."""
    _setFramesArray(prim, [(start, end)])


def setStartFrame(frame):
    """Set the start frame on the active render settings prim's adsk:frames.

    If adsk:frames is already authored, updates only the start value of the
    first element.  If not authored, creates a single (start, start) entry.

    Raises RuntimeError if the frame is invalid (non-numeric) or if it
    exceeds the current end frame."""
    try:
        frame_val = float(frame)
    except (TypeError, ValueError):
        raise RuntimeError(
            "Start frame must be numeric, got: %s" % frame)

    prim = getRenderSettingsPrim()
    existing = _getFramesPair(prim)

    if existing is not None:
        _start, end = existing
        if frame_val > end:
            raise RuntimeError(
                "Start frame (%s) cannot exceed end frame (%s)."
                % (frame_val, end))
        _setFramesPair(prim, frame_val, end)
    else:
        _setFramesPair(prim, frame_val, frame_val)

    _log.info("Set start frame to %.4g", frame_val)


def setEndFrame(frame):
    """Set the end frame on the active render settings prim's adsk:frames.

    If adsk:frames is already authored, updates only the end value of the
    first element.  If not authored, creates a single (end, end) entry.

    Raises RuntimeError if the frame is invalid (non-numeric) or if it
    is below the current start frame."""
    try:
        frame_val = float(frame)
    except (TypeError, ValueError):
        raise RuntimeError(
            "End frame must be numeric, got: %s" % frame)

    prim = getRenderSettingsPrim()
    existing = _getFramesPair(prim)

    if existing is not None:
        start, _end = existing
        if frame_val < start:
            raise RuntimeError(
                "End frame (%s) cannot be less than start frame (%s)."
                % (frame_val, start))
        _setFramesPair(prim, start, frame_val)
    else:
        _setFramesPair(prim, frame_val, frame_val)

    _log.info("Set end frame to %.4g", frame_val)


def setStep(step):
    """Set the frame step on the active render settings prim's adsk:step.

    Raises RuntimeError if step is non-numeric or less than or equal to 0."""
    try:
        step_val = float(step)
    except (TypeError, ValueError):
        raise RuntimeError(
            "Step must be numeric, got: %s" % step)

    if step_val <= 0:
        raise RuntimeError(
            "Step must be greater than 0, got: %s" % step_val)

    prim = getRenderSettingsPrim()
    attr = prim.GetAttribute(_STEP_ATTR)
    if not attr:
        attr = prim.CreateAttribute(
            _STEP_ATTR, Sdf.ValueTypeNames.Float, custom=True)
    attr.Set(step_val)

    _log.info("Set step to %.4g", step_val)

def setFrames(frames):
    """Set the active render settings prim's adsk:frames attribute.

    frames is a comma-separated list of items, e.g. "1, 2, 5-10, 15, 30-50".
    Each item is either a single frame, expanded into a pair of identical
    values, or a hyphen-separated pair of frames.  Frames may be negative or
    zero.

    If adsk:frames is already authored, it is replaced.  If not authored, 
    it will be created.

    Raises RuntimeError if the list is malformed, if a pair ends before it
    starts, or if an item does not start strictly after the preceding item."""
    pairs = _parseFrames(frames)

    prim = getRenderSettingsPrim()
    _setFramesArray(prim, pairs)

    _log.info("Set frames to %s", pairs)
