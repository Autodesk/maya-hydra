#
# Copyright 2024 Autodesk, Inc. All rights reserved.
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
import fixturesUtils
import mtohUtils
import mayaUtils
import unittest
from testUtils import PluginLoaded

import platform

class TestMayaDisplayModes(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 0.1
        return 0.01
    
    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 4
        return 0.2

    def test_MayaDisplayModes(self):

        # open simple Maya scene
        testFile = mayaUtils.openTestScene(
                "testMayaDisplayModes",
                "testMayaDisplayModes.ma")
        cmds.refresh()
        panel = mayaUtils.activeModelPanel()
        
        #start with default lights        
        cmds.modelEditor(panel, edit=True, displayLights="default")
        
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=False)

        #Use Default Material
        cmds.modelEditor(panel, edit=True, useDefaultMaterial=True)
        cmds.refresh()
        self.assertSnapshotClose("defaultMaterial" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)        
        cmds.modelEditor(panel, edit=True, useDefaultMaterial=False)

        #xray mode
        cmds.modelEditor(panel, edit=True, xray=True)
        cmds.refresh()
        self.assertSnapshotClose("xray" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)        
        cmds.modelEditor(panel, edit=True, xray=False)
        
        #WireframeOnShaded mode
        cmds.modelEditor(panel, edit=True, useDefaultMaterial=True)
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=True)
        cmds.refresh()
        self.assertSnapshotClose("wireframeOnShaded" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=False)
        cmds.modelEditor(panel, edit=True, useDefaultMaterial=False)

        #All Lights mode
        cmds.modelEditor(panel, edit=True, displayLights="all")
        cmds.refresh()
        self.assertSnapshotClose("allLights" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        
        #Shadows mode
        cmds.modelEditor(panel, edit=True, shadows=True)
        cmds.refresh()
        self.assertSnapshotClose("shadows" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, shadows=False)
        
        #Smoothshaded Material
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")
        cmds.refresh()
        self.assertSnapshotClose("smoothShaded" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)        
        cmds.modelEditor(panel, edit=True, displayLights="default")   #revert to default lighting 

        #Display texures mode         
        cmds.modelEditor(panel, edit=True, displayTextures=True)  
        cmds.refresh()
        self.assertSnapshotClose("displayTextures" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)     
        cmds.modelEditor(panel, edit=True, displayTextures=False)

    @unittest.skipUnless(mtohUtils.checkForMayaUsdPlugin(), "Requires Maya USD Plugin.")
    def test_MayaBoundingBoxDisplayMode(self):
        with PluginLoaded('mayaHydraFlowViewportAPILocator'):
            # open a Maya scene that contains USD prims, custom flow viewport data producer prims and maya native prims
            testFile = mayaUtils.openTestScene(
                    "testMayaDisplayModes",
                    "testMayaBoundingBoxDisplayMode.ma")
            cmds.refresh()
            self.assertSnapshotClose("BBox_sceneLoaded" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            panel = mayaUtils.activeModelPanel()
            cmds.modelEditor(panel, edit=True, displayAppearance="boundingBox")
            self.assertSnapshotClose("BBox_DisplayModeOn" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
    
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
