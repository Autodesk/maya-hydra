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

class TestStandardSurface(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2

    #Test the translation from maya standard surface with a maya native plane to usd preview surface.
    def test_StandardSurface(self):
        cmds.file(new=True, force=True)

        # Load a maya scene with a maya native plane, which has autodesk standard surface as material
        testFile = mayaUtils.openTestScene(
                "testStandardSurface",
                "testStandardSurface.ma")
        cmds.refresh()

        #Verify Default display
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayTextures=True)
        cmds.refresh()
        self.assertSnapshotClose("default" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Diffuse Roughness
        cmds.setAttr("standardSurface1.diffuseRoughness", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("diffuseRoughness" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Metalness
        cmds.setAttr("standardSurface1.metalness", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("metalness" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Specular Roughness
        cmds.setAttr("standardSurface1.specularRoughness", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("specularRoughness" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Specular IOR
        cmds.setAttr("standardSurface1.specularIOR", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("specularIOR" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Transmission
        cmds.setAttr("standardSurface1.transmission", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("transmission" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        #Verify Emission
        cmds.setAttr("standardSurface1.emission", 0.5)
        cmds.refresh()
        self.assertSnapshotClose("emission" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
