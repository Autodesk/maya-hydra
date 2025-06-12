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
import os

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
        def assertSnapshotCloseImpl(img_name:str, image_version: str | None = None):
            self.assertSnapshotClose(f'{img_name}.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, imageVersion=image_version)

        image_version = 'USD2505+' if self._usdVersion >= (0, 25, 5) else None

        # Load a maya scene with a maya native plane, which has autodesk OpenPBR surface as material
        testFile = mayaUtils.openTestScene(
                "testOpenPBRSurface",
                "testOpenPBRSurface.ma")
        cmds.refresh()

        #Verify Default display
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, displayTextures=True)
        cmds.refresh()
        assertSnapshotCloseImpl("default")

        #Disconnect the texture
        cmds.disconnectAttr("file1.outColor", "openPBRSurface1.baseColor")
        cmds.refresh()
        assertSnapshotCloseImpl("default_noTexture")

        #Verify Base
        cmds.setAttr("openPBRSurface1.baseColor", 0.5,0.0,0.0, type = 'double3')
        cmds.refresh()
        assertSnapshotCloseImpl("baseColor")
        cmds.setAttr("openPBRSurface1.baseWeight", 0.5)
        cmds.refresh()
        assertSnapshotCloseImpl("baseWeight")

        #Verify Metalness
        cmds.setAttr("openPBRSurface1.baseMetalness", 0.5)
        cmds.refresh()
        assertSnapshotCloseImpl("metalness")

        #Verify Specular
        cmds.setAttr("openPBRSurface1.specularColor", 0.0,0.0,0.5, type = 'double3')
        cmds.refresh()
        assertSnapshotCloseImpl("specularColor")
        cmds.setAttr("openPBRSurface1.specularWeight", 0.2)
        cmds.refresh()
        assertSnapshotCloseImpl("specularWeight")
        cmds.setAttr("openPBRSurface1.specularRoughness", 0.7)
        cmds.refresh()
        assertSnapshotCloseImpl("specularRoughness")
        cmds.setAttr("openPBRSurface1.specularIOR", 0.5)
        cmds.refresh()
        assertSnapshotCloseImpl("specularIOR")

        #Verify Emission
        cmds.setAttr("openPBRSurface1.emissionLuminance", 500.0)
        cmds.refresh()
        assertSnapshotCloseImpl("emissionLuminance")
        cmds.setAttr("openPBRSurface1.emissionColor", 0.0,0.5,0.0, type = 'double3')
        cmds.refresh()
        assertSnapshotCloseImpl("emissionColor")

        #Verify Transmission
        cmds.setAttr("openPBRSurface1.transmissionWeight", 0.5)
        cmds.refresh()
        assertSnapshotCloseImpl("transmissionWeight", image_version)

        #Verify Opacity
        cmds.setAttr("openPBRSurface1.geometryOpacity", 0.2)
        cmds.refresh()
        assertSnapshotCloseImpl("geometryOpacity", image_version)

        #Verify Coat
        if(os.getenv('MAYA_HAS_RENDER_ITEM_CULL_MODE_API', 'NOT-FOUND') in ('1', 'TRUE')):
            image_version = "RenderItemHasCullModeAPI"
        cmds.setAttr("openPBRSurface1.coatWeight", 0.9)
        cmds.refresh()
        assertSnapshotCloseImpl("coatWeight", image_version)
        cmds.setAttr("openPBRSurface1.coatColor", 0.0,0.0,0.0, type = 'double3')
        cmds.refresh()
        assertSnapshotCloseImpl("coatColor", image_version)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
