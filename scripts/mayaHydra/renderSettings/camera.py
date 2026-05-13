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

from pxr import Sdf, UsdRender

def setCamera(cameraPath):
    rsStagePathStr = "|renderSettings|renderSettingsShape"
    stage = mayaUsdUfe.getStage(rsStagePathStr)

    if not stage:
        raise RuntimeError("No stage found at %s, setCamera() failed." % rsStagePathStr)

    rsParentPrimPath = Sdf.Path("/Render")
    rsParentPrim = stage.GetPrimAtPath(rsParentPrimPath)

    if not rsParentPrim:
        raise RuntimeError("Render settings parent prim %s not found." % str(rsParentPrimPath))

    for child in rsParentPrim.GetChildren():
        if child.IsA(UsdRender.Product):
            attr = child.CreateAttribute("adskUsd:externalCamera", Sdf.ValueTypeNames.String)
            attr.Set(cameraPath)
