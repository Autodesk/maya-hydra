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

from .utils import getRenderSettingsPrim

import mayaUsd.ufe as mayaUsdUfe
import maya.cmds as cmds

from pxr import Sdf, UsdRender, Gf

DEFAULT_WIDTH  = 960
DEFAULT_HEIGHT = 540

def getResolutionAttr():
    rsPrim = getRenderSettingsPrim()

    if not rsPrim:
        raise RuntimeError("Render settings prim %s not found." % str(rsPrim.GetPath()))

    # rs is a UsdRender.Settings schema object.
    rs = UsdRender.Settings(rsPrim)

    if not rs:
        raise RuntimeError("No render settings found under %s." % str(rsParentPrimPath))

    resAttr = rs.GetResolutionAttr()
    if (not resAttr):
        resAttr = rs.CreateResolutionAttr(
            (DEFAULT_WIDTH, DEFAULT_HEIGHT))

    return resAttr


def setWidth(width):
    cmds.setAttr("defaultResolution.width", width)

    try:
        resAttr = getResolutionAttr()
    except RuntimeError as err:
        if "No stage found" in str(err):
            return
        raise
    w, h = resAttr.Get()

    # Passing a Python integer 2-tuple directly to Set() causes
    # a Gf.Vec2d() to be created, and a type mismatch when the
    # Python C++ binding is called.
    newRes = Gf.Vec2i(width, h)
    resAttr.Set(newRes)

def setHeight(height):
    cmds.setAttr("defaultResolution.height", height)

    try:
        resAttr = getResolutionAttr()
    except RuntimeError as err:
        if "No stage found" in str(err):
            return
        raise
    w, h = resAttr.Get()

    # See setWidth() Gf.Vec2i comments.
    newRes = Gf.Vec2i(w, height)
    resAttr.Set(newRes)
