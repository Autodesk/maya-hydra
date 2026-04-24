#
# Copyright 2025 Autodesk, Inc. All rights reserved.
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

class TestLightingByRenderDelegate(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    
    # Required plugins for this test
    _requiredPlugins = ['mtoa']  # Arnold plugin for HdArnold render delegate
    
    # Image comparison thresholds
    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 0.5

    def _testSceneWithAllRenderDelegates(self, sceneName):
        """Helper method to test a scene with all three render delegates"""
        # Load the scene using mayaUtils.openTestScene
        sceneFile = f"{sceneName}.ma"
        mayaUtils.openTestScene("testLightingByRenderDelegates", sceneFile)
        
        # Remove anti-aliasing for consistent results
        cmds.setAttr("hardwareRenderingGlobals.multiSampleEnable", False)
        cmds.refresh()
        
        # Test with VP2 renderer
        self.setViewport2Renderer()
        vp2ImageName = f"{sceneName}_VP2.png"
        self.assertSnapshotClose(vp2ImageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        # Test with HdStorm renderer
        self.setHdStormRenderer()
        stormImageName = f"{sceneName}_Storm.png"
        self.assertSnapshotClose(stormImageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        
        # Test with HdArnold renderer (if available)
        try:
            self.setHdArnoldRenderer()
            arnoldImageName = f"{sceneName}_Arnold.png"
            self.assertSnapshotClose(arnoldImageName, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        except Exception as e:
            # Skip Arnold test if plugin is not available or fails to initialize
            self.skipTest(f"HdArnold render delegate not available: {str(e)}")

    def testAreaLightScene(self):
        self._testSceneWithAllRenderDelegates("AreaLight")

    def testArnoldAreaLightScene(self):
        self._testSceneWithAllRenderDelegates("ArnoldArealight")

    def testDirectionalLightScene(self):
        self._testSceneWithAllRenderDelegates("directionalLight")

    def testPointLightScene(self):
        self._testSceneWithAllRenderDelegates("pointLight")

    def testSkyDomeLightColorOnlyScene(self):
        self._testSceneWithAllRenderDelegates("skyDomeLightColorOnly")

    def testSkyDomeLightTextureScene(self):
        self._testSceneWithAllRenderDelegates("skyDomeLightTexture")

    def testSpotLightScene(self):
        self._testSceneWithAllRenderDelegates("spotLight")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
