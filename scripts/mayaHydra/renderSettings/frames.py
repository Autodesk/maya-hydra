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

from pxr import Gf, Sdf, Vt

from .utils import getRenderSettingsPrim

_log = logging.getLogger(__name__)

_FRAMES_ATTR = "adsk:frames"


def _getFramesPair(prim):
    """Return (start, end) from the first element of adsk:frames, or None."""
    attr = prim.GetAttribute(_FRAMES_ATTR)
    if attr and attr.HasAuthoredValue():
        frames = attr.Get()
        if frames is not None and len(frames) > 0:
            return (frames[0][0], frames[0][1])
    return None


def _setFramesPair(prim, start, end):
    """Write a single-element double2[] to adsk:frames on the prim."""
    attr = prim.GetAttribute(_FRAMES_ATTR)
    if not attr:
        attr = prim.CreateAttribute(
            _FRAMES_ATTR, Sdf.ValueTypeNames.Double2Array, custom=True)
    attr.Set(Vt.Vec2dArray([Gf.Vec2d(start, end)]))


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
