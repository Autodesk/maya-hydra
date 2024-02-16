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

import platform

class TestMayaShadingModes(mtohUtils.MtohTestCase): #Subclassing mtohUtils.MtohTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        return 0.01
    
    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 3
        return 0.2

    def test_MayaShadingModes(self):
        
        testFile = mayaUtils.openTestScene(
                "testMayaShadingModes",
                "testMayaShadingModes.ma")
        
        cmds.refresh()

        panel = mayaUtils.activeModelPanel()        
        
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=False)
        #Smooth Shading
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")
        cmds.refresh()
        self.assertSnapshotClose("default" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Flat Shading
        cmds.modelEditor(panel, edit=True, displayAppearance="flatShaded")
        cmds.refresh()
        self.assertSnapshotClose("flatShaded" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")

        #Bounding Box
        cmds.modelEditor(panel, edit=True, displayAppearance="boundingBox")
        cmds.refresh()
        self.assertSnapshotClose("boundingBox" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")

        #Wirefame
        cmds.modelEditor(panel, edit=True, smoothWireframe=True)
        cmds.refresh()
        self.assertSnapshotClose("wireframe" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, smoothWireframe=False)

        #X-ray
        cmds.modelEditor(panel, edit=True, xray=True)
        cmds.refresh()
        self.assertSnapshotClose("xray" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)        
        cmds.modelEditor(panel, edit=True, xray=False)

        #joint xray mode
        cmds.modelEditor(panel, edit=True, jointXray=True)
        cmds.refresh()
        self.assertSnapshotClose("jointxray" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        cmds.modelEditor(panel, edit=True, jointXray=False)

        #backfaceCulling  
        cmds.modelEditor(panel, edit=True, backfaceCulling=True)
        cmds.refresh()
        self.assertSnapshotClose("backfaceCulling" + ".png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
