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

# Translates Maya's `-reg L R B T` crop-region flag into
# UsdRenderProduct.dataWindowNDC and writes it to every active render product.
#
# Maya `Render -reg L R B T` is in pixel coordinates: left, right, bottom,
# top, INCLUSIVE on all four sides, Y-up, origin bottom-left.  This matches
# what the Maya Software renderer itself does
# (Maya/src/RenderSlice/RenderInterface/renderer/TrenderFrames.cpp clamps
# `xhigh = image_width - 1` and uses that as the no-region default).
#
# USD UsdRenderProduct.dataWindowNDC is a GfVec4f(xmin, ymin, xmax, ymax) in
# normalized aperture coordinates [0, 1], also Y-up, origin bottom-left,
# default (0, 0, 1, 1) = full image.
#
# Conversion of inclusive pixel bounds to NDC is therefore:
#
#     xmin =  left          / W
#     ymin =  bottom        / H
#     xmax = (right  + 1)   / W   # +1 because `right` is INCLUSIVE
#     ymax = (top    + 1)   / H   # +1 because `top`   is INCLUSIVE
#
# This round-trips exactly through HdPrman's own NDC-to-pixel conversion in
# `third_party/renderman-{26,27}/plugin/hdPrman/renderSettings.cpp`:
#
#     GfRect2i dataWindow(
#         round(ndc.GetMin() * resolution),
#         round(ndc.GetMax() * resolution) - GfVec2i(1));   // INCLUSIVE rect

from pxr import Gf

from . import renderProducts
from . import resolution


def _toDataWindowNDC(left, right, bottom, top, width, height):
    """Translate inclusive Maya pixel bounds to a USD dataWindowNDC GfVec4f.

    Raises RuntimeError on an invalid resolution or an empty/inverted
    region (right < left or top < bottom)."""
    if width <= 0 or height <= 0:
        raise RuntimeError(
            "Invalid render resolution %dx%d." % (width, height))

    # Reject out-of-bounds coordinates before clamping so the error message
    # is accurate (clamping first would turn an out-of-bounds input into a
    # degenerate xmin==xmax and report a misleading "require left <= right").
    if left < 0 or bottom < 0 or right >= width or top >= height:
        raise RuntimeError(
            "Crop region out of image bounds: left=%d right=%d bottom=%d top=%d "
            "(image %dx%d, valid range [0, %d] x [0, %d])."
            % (left, right, bottom, top, width, height, width - 1, height - 1))

    if right < left or top < bottom:
        raise RuntimeError(
            "Invalid crop region: left=%d right=%d bottom=%d top=%d "
            "(image %dx%d). Bounds are inclusive; require "
            "left <= right and bottom <= top."
            % (left, right, bottom, top, width, height))

    xmin =  left          / float(width)
    ymin =  bottom        / float(height)
    xmax = (right  + 1)   / float(width)
    ymax = (top    + 1)   / float(height)

    return Gf.Vec4f(xmin, ymin, xmax, ymax)


def setRegion(left, right, bottom, top):
    """Set the crop region for the active Maya Hydra render settings.

    Arguments are inclusive pixel coordinates (left, right, bottom, top),
    Y-up, origin bottom-left, matching the Maya `Render -reg L R B T` flag.

    Translates them to UsdRenderProduct.dataWindowNDC and applies the value
    to every render product picked by
    renderProducts.getRenderProductsToApplySettings()."""
    resAttr = resolution.getResolutionAttr()
    width, height = resAttr.Get()

    dataWindowNDC = _toDataWindowNDC(
        left, right, bottom, top, width, height)

    for rp in renderProducts.getRenderProductsToApplySettings():
        # CreateDataWindowNDCAttr returns the existing attribute when it already
        # exists, or creates it with the given default value.  The explicit
        # attr.Set() is required for the update path: Create() does not change
        # the value of an attribute that already exists.
        attr = rp.CreateDataWindowNDCAttr(dataWindowNDC)
        if not attr:
            raise RuntimeError(
                "Could not obtain a dataWindowNDC attribute for render "
                "product %s." % str(rp.GetPrim().GetPath()))
        attr.Set(dataWindowNDC)
