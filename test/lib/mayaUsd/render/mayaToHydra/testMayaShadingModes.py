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

class TestMayaShadingModes(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.3

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
        self.assertSnapshotClose("default" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Flat Shading
        cmds.modelEditor(panel, edit=True, displayAppearance="flatShaded")
        cmds.refresh()
        self.assertSnapshotClose("flatShaded" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")

        #Bounding Box
        cmds.modelEditor(panel, edit=True, displayAppearance="boundingBox")
        cmds.refresh()
        self.assertSnapshotClose("boundingBox" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")

        #SmoothWirefame
        cmds.modelEditor(panel, edit=True, displayAppearance="wireframe")
        cmds.modelEditor(panel, edit=True, smoothWireframe=True)
        cmds.refresh()
        self.assertSnapshotClose("smoothwireframe" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.modelEditor(panel, edit=True, smoothWireframe=False)
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")

        #SmoothWirefameOnShaded
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=True)
        cmds.modelEditor(panel, edit=True, smoothWireframe=True)
        cmds.refresh()
        self.assertSnapshotClose("smoothwireframeonshaded" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.modelEditor(panel, edit=True, smoothWireframe=False)
        cmds.modelEditor(panel, edit=True, wireframeOnShaded=False)

        #X-ray
        cmds.modelEditor(panel, edit=True, xray=True)
        cmds.refresh()
        self.assertSnapshotClose("xray" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)        
        cmds.modelEditor(panel, edit=True, xray=False)
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
