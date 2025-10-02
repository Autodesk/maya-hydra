# Copyright 2025 Autodesk
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
import maya.mel

import fixturesUtils
import mtohUtils
import mayaUsd
import mayaUtils

class TestPurposeRenderTag(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1
    _stage = None

    def loadUsdScene(self):
        import usdUtils
        import testUtils
        usdScenePath = testUtils.getTestScene('testDefaultMaterial', 'PoolBallFlat_animated.usdz')
        proxyShapePathStr = usdUtils.createStageFromFile(usdScenePath)
        self._stage = mayaUsd.lib.GetPrim(proxyShapePathStr).GetStage()
        self.setHdStormRenderer()
        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayTextures=True)  
        self.setBasicCam(5)
        cmds.refresh()

    def test_purposeRenderTag(self):
        self.loadUsdScene()
        cmds.select(clear=True)

        #Reset all to on
        cmds.setAttr("defaultRenderGlobals.mayaHydraProxyPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraProxyPurpose")
        cmds.setAttr("defaultRenderGlobals.mayaHydraRenderPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRenderPurpose")
        cmds.setAttr("defaultRenderGlobals.mayaHydraGuidePurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraGuidePurpose")

        # Modify the purpose of the prim
        ball_purpose = self._stage.GetPrimAtPath('/beach/ball').GetAttribute('purpose')
        self.assertIsNotNone(ball_purpose)
        #Set purpose render tag as Proxy for the stage prim
        ball_purpose.Set('proxy')
        self.assertEqual(ball_purpose.Get(), 'proxy')
        self.assertSnapshotClose("AsProxy_ProxyOn.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)
        #Disable proxy display
        cmds.setAttr("defaultRenderGlobals.mayaHydraProxyPurpose", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraProxyPurpose")
        self.assertSnapshotClose("AsProxy_ProxyOff.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)
        ball_purpose.Set('render')
        self.assertEqual(ball_purpose.Get(), 'render')
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRenderPurpose")
        self.assertSnapshotClose("AsRender_ProxyOff.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)
        #Enable proxy display
        cmds.setAttr("defaultRenderGlobals.mayaHydraProxyPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraProxyPurpose")
        #Disable render display
        cmds.setAttr("defaultRenderGlobals.mayaHydraRenderPurpose", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRenderPurpose")
        self.assertSnapshotClose("AsRender_RenderOff.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)
        ball_purpose.Set('guide')#This will make the prim go to the secondary graphics
        self.assertEqual(ball_purpose.Get(), 'guide')
        cmds.mayaHydra(updateRenderGlobals="mayaHydraGuidePurpose")
        self.assertSnapshotClose("AsGuide_RenderOff.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)
        #Enable render display
        cmds.setAttr("defaultRenderGlobals.mayaHydraRenderPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRenderPurpose")
        #Disable guide display
        cmds.setAttr("defaultRenderGlobals.mayaHydraGuidePurpose", 0)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraGuidePurpose")
        self.assertSnapshotClose("AsGuide_GuideOff.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

        #Reset all to on
        cmds.setAttr("defaultRenderGlobals.mayaHydraProxyPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraProxyPurpose")
        cmds.setAttr("defaultRenderGlobals.mayaHydraRenderPurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraRenderPurpose")
        cmds.setAttr("defaultRenderGlobals.mayaHydraGuidePurpose", 1)
        cmds.mayaHydra(updateRenderGlobals="mayaHydraGuidePurpose")

        self.assertSnapshotClose("AsGuide_EverythingOn.png", self.IMAGEDIFF_FAIL_THRESHOLD, self.IMAGEDIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
