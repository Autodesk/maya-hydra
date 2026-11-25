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

import maya.cmds as cmds

import mayaUsd.ufe as mayaUsdUfe
from pxr import UsdRender

_NODE = 'UsdDefaultRenderDescription'


def setActiveRenderDescription(ufePath):
    if not ufePath:
        raise RuntimeError("Cannot set activeRenderDescriptionPath: the UFE path is empty.")
    if not cmds.objExists(_NODE):
        raise RuntimeError(
            "Cannot set activeRenderDescriptionPath: node %s not found in the scene."
            % _NODE)

    errorMsg = "No render description prim found at path %s" % ufePath
    try:
        prim = mayaUsdUfe.ufePathToPrim(ufePath)
    except RuntimeError:
        raise RuntimeError(errorMsg)
    if not prim or not (prim.IsA(UsdRender.Settings) or prim.IsA(UsdRender.Pass)):
        raise RuntimeError(errorMsg)

    cmds.setAttr('%s.activeRenderDescriptionPath' % _NODE, ufePath, type='string')
