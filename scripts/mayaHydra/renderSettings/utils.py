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

import mayaUsd.ufe as mayaUsdUfe

from maya import cmds

from pxr import UsdRender

def getRenderSettingsPrim():
    rsPath = cmds.getAttr('UsdDefaultRenderDescription.activeRenderDescriptionPath')
    renderDescriptionPrim = mayaUsdUfe.ufePathToPrim(rsPath)

    # 1. If the attribute points to a UsdRenderSettings prim, use it directly.
    if renderDescriptionPrim.IsA(UsdRender.Settings):
        return renderDescriptionPrim

    # 2. If the attribute points to a UsdRenderPass prim, resolve its
    # renderSource relationship to the referenced UsdRenderSettings prim.
    if renderDescriptionPrim.IsA(UsdRender.Pass):
        targets = UsdRender.Pass(renderDescriptionPrim).GetRenderSourceRel().GetTargets()

        # 2.1 Use the render settings only when renderSource has exactly one
        # valid UsdRenderSettings target.
        if len(targets) == 1:
            renderSettingsPrim = renderDescriptionPrim.GetStage().GetPrimAtPath(targets[0])
            if renderSettingsPrim.IsA(UsdRender.Settings):
                return renderSettingsPrim

    # 2.2 / 2.3. An invalid renderSource falls back to
    # the default render settings provided by UsdDefaultRenderDescription.
    defaultRenderSettingsPath = "UsdDefaultRenderDescription,/Render/SceneRenderSettings"
    return mayaUsdUfe.ufePathToPrim(defaultRenderSettingsPath)