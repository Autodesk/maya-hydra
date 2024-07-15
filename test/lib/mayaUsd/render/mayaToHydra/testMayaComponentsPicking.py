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

class TestMayaComponentsPicking(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def test_MayaFaceComponentsPicking(self):
        
        # load a maya scene
        testFile = mayaUtils.openTestScene(
                "testMayaComponentsPicking",
                "testMayaComponentsPicking.ma", useTestSettings=False)
        cmds.refresh()
        
        #We are in VP2, select some faces
        cmds.select( 'pSphere1.f[1:300]', r=True )
        self.assertSnapshotClose("facesSelVP2" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        self.setHdStormRenderer()
        self.assertSnapshotClose("facesSelStorm" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.select( 'pSphere1.f[236]', r=True )
        self.assertSnapshotClose("face236SelStorm" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Check in wireframe
        cmds.select( 'pSphere1.f[1:200]', r=True )
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayAppearance="wireframe")
        cmds.modelEditor(panel, edit=True, smoothWireframe=True)
        cmds.refresh()
        self.assertSnapshotClose("smoothwireframe" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Add lights
        cmds.directionalLight(rotation=(45, 30, 15)) 
        cmds.modelEditor(panel, edit=True, displayAppearance="smoothShaded")
        cmds.modelEditor(panel, edit=True, displayLights="all")        
        cmds.select( 'pSphere1.f[1:200]', r=True )
        cmds.refresh()
        self.assertSnapshotClose("selectionWithLights" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
if __name__ == '__main__':
    fixturesUtils.runTests(globals())
