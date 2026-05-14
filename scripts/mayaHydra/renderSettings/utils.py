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

def getRenderSettingsPrim():
    # Get the UFE path to the render settings prim to use from the
    # default USD render settings node.
    rsPath = cmds.getAttr('UsdDefaultRenderSettings.activeSettingsPath')

    return mayaUsdUfe.ufePathToPrim(rsPath)
