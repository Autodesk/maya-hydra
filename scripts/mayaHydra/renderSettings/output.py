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
import maya.mel as mel

from pathlib import Path

from . import renderProducts


# Render settings are written both to Maya render settings and USD render
# settings to ease transition to USD render settings.

def setRenderDirectory(rd):
    # Set the Maya render settings.
    cmds.workspace(fr=['depth', rd])
    cmds.workspace(fr=['images', rd])

    # Given an input product name, map it to our argument directory.
    def setRenderProductName(productName):
        return str(Path(rd) / Path(productName).name)

    # Set the directory on every USD render product picked by the shared
    # selector.
    renderProducts.applyToProductName(setRenderProductName)

def setImageName(im):
    # Set the Maya render setting.
    cmds.setAttr('defaultRenderGlobals.imageFilePrefix', im, type='string')

    def setRenderProductName(productName):
        p = Path(productName)
        return str(p.with_name(im + p.suffix))

    # Set the file name on every USD render product picked by the shared
    # selector.
    renderProducts.applyToProductName(setRenderProductName)

def setOutputFormat(of):
    # Set the Maya render setting.
    mel.eval('setMayaSoftwareImageFormat("' + of + '")')
    cmds.setAttr('defaultArnoldDriver.aiTranslator', of, type='string')

    def setRenderProductName(productName):
        return str(Path(productName).with_suffix('.' + of))

    # Set the output format on every USD render product picked by the shared
    # selector.
    renderProducts.applyToProductName(setRenderProductName)
