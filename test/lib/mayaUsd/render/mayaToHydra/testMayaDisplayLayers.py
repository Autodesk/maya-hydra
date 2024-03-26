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

import platform

class TestMayaDisplayLayers(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
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
    
    def setLayerDisplayType(self, val):        
        allLayers = cmds.ls(long=True, type='displayLayer')
        for l in allLayers:
            if l != "defaultLayer":
                cmds.setAttr(l+".displayType", val)


    def test_MayaDisplayLayers(self):

        sphere = cmds.sphere( n='sphere1' )
        cone = cmds.cone( n='cone1' )
        cmds.move( 1, 5, 1 )
        cmds.select(None)
        self.assertSnapshotClose("default" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        
        cmds.select(sphere)
        # Create the Display layer for Sphere
        sphereLayer = cmds.createDisplayLayer(noRecurse=True, name='SphereLayer') 
        cmds.select(None)

        cmds.refresh()
        cmds.select(cone)
        # Create the Display layer for Cone
        conePlayer = cmds.createDisplayLayer(noRecurse=True, name='ConeLayer')        
        cmds.select(None)
        cmds.refresh()
        # All layers  
        self.assertSnapshotClose("allLayers" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        # Hide Sphere
        cmds.setAttr(sphereLayer+".visibility", False)
        cmds.refresh()

        # Only Cone  
        self.assertSnapshotClose("coneLayer" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        # Unhide Sphere
        cmds.setAttr(sphereLayer+".visibility", True)
        cmds.refresh()

        # Custom Wireframe Color for Cone layer
        cmds.setAttr(conePlayer+".levelOfDetail", 1)
        cmds.setAttr(conePlayer+".color", 11)
        self.assertSnapshotClose("coneLayerCustomWireframe" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.setAttr(conePlayer+".levelOfDetail", 0)
        cmds.refresh()

        # Display types
        
        # Template        
        cmds.select(None)
        self.setLayerDisplayType(1)
        self.assertSnapshotClose("layerTemplate" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)        
        cmds.refresh()
        cmds.select(sphere)
        self.assertSnapshotClose("layerTemplateSelected" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.select(None)
        
        # Reference
        self.setLayerDisplayType(2)
        self.assertSnapshotClose("layerReference" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)      
        cmds.refresh()
        cmds.select(sphere)
        self.assertSnapshotClose("layerReferenceSelected" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.select(None)

        # reset to normal
        self.setLayerDisplayType(0)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())