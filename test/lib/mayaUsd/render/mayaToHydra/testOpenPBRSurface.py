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

class TestOpenPBRSurface(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        # Use a larger tolerance for transparency on OSX
        if platform.system() == "Darwin":
            return 2
        return 0.2

    #Test the translation from maya OpenPBR surface with a maya native plane to usd preview surface.
    def test_OpenPBRSurface(self):
        # Load a maya scene with a maya native plane, which has autodesk OpenPBR surface as material
        testFile = mayaUtils.openTestScene(
                "testOpenPBRSurface",
                "testOpenPBRSurface.ma")
        cmds.refresh()

        #Verify Default display
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayTextures=True)
        cmds.refresh()
        self.assertSnapshotClose("default" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Disconnect the texture
        cmds.disconnectAttr("file1.outColor", "openPBRSurface1.baseColor")
        cmds.refresh()
        self.assertSnapshotClose("default_noTexture" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Base
        cmds.setAttr("openPBRSurface1.baseColor", 0.5,0.0,0.0, type = 'double3')
        cmds.refresh()
        self.assertSnapshotClose("baseColor" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.baseWeight", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("baseWeight" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Metalness
        cmds.setAttr("openPBRSurface1.baseMetalness", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("metalness" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Specular
        cmds.setAttr("openPBRSurface1.specularColor", 0.0,0.0,5.0, type = 'double3')
        cmds.refresh()
        self.assertSnapshotClose("specularColor" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.specularWeight", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("specularWeight" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.specularRoughness", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("specularRoughness" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.specularIOR", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("specularIOR" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Transmission
        cmds.setAttr("openPBRSurface1.transmissionWeight", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("transmissionWeight" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.geometryOpacity", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("geometryOpacity" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Emission
        cmds.setAttr("openPBRSurface1.emissionColor", 0.0,0.5,0.0, type = 'double3')
        cmds.refresh()
        self.assertSnapshotClose("emissionColor" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        cmds.setAttr("openPBRSurface1.emissionLuminance", 500.0)
        cmds.refresh()
        self.assertSnapshotClose("emissionLuminance" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
