#
# Copyright 2023 Autodesk, Inc. All rights reserved.
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

import fixturesUtils
import mayaUtils
import mtohUtils

import unittest

from testUtils import PluginLoaded

class TestColorPreferences(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Utils
    def resetColorPrefs(self):
        mel.eval("displayRGBColor -rf; displayColor -rf; colorIndex -rf;")

    def setUp(self):
        self.resetColorPrefs()
        self.setHdStormRenderer()
    
    def tearDown(self):
        self.resetColorPrefs()

    # Individual tests
    def test_rgbaColorNotification(self):
        self.runCppTest("ColorPreferences.rgbaColorNotification")

    @unittest.skipUnless(mayaUtils.hydraFixLevel() > 1, "Requires DisplayColorChanged event bugfix.")
    def test_indexedColorNotification(self):
        self.runCppTest("ColorPreferences.indexedColorNotification")

    def test_paletteColorNotification(self):
        self.runCppTest("ColorPreferences.paletteColorNotification")

    def test_rgbaColorQuery(self):
        self.runCppTest("ColorPreferences.rgbaColorQuery")

    @unittest.skipUnless(mayaUtils.hydraFixLevel() > 1, "Requires DisplayColorChanged event bugfix.")
    def test_indexedColorQuery(self):
        self.runCppTest("ColorPreferences.indexedColorQuery")
    
    def test_paletteColorQuery(self):
        self.runCppTest("ColorPreferences.paletteColorQuery")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
