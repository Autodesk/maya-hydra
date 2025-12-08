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
from testUtils import PluginLoaded
import platform

class TestFramePasses(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01

    @property
    def IMAGE_DIFF_FAIL_PERCENT(self):
        if platform.system() == "Darwin":
            return 3
        return 2 #We have errors on Windows and Linux of about 1%, so we need to increase the threshold

    def setUp(self):
        super(TestFramePasses, self).setUp()

    def run_assertion(self, filename, mode, failures):
        try:
            self.assertSnapshotClose(filename, self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
        except AssertionError as e:
            failures.append(f"Failed on {filename} ({mode}): {str(e)}")
            # Still continue to the next test
    
    def SetAllPassesVisibleToColorAOV(self):
        # Enable all frame passes
        cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=[0, 1])
        #Workaround for a bug when re-enabling all passes
        self.setViewport2Renderer()
        self.setHdStormRenderer()

    #Enable only pass #0 then pass #1 and do snapshots using as an option the aov namre specified
    def EnablePassesOneByOneAndDoSnapshots(self, name, failures, aovName=""):
        # Enable only pass#0
        kwargs = {"edit": True, "visible": [0]}
        if aovName:
            kwargs["aovName"] = aovName
        cmds.mayaHydraSetVisibleFramePasses(**kwargs)
        self.run_assertion(("FirstPass"+name+".png"), ("first pass "+name), failures)
    
        # Enable only pass#1
        kwargs = {"edit": True, "visible": [1]}
        if aovName:
            kwargs["aovName"] = aovName
        cmds.mayaHydraSetVisibleFramePasses(**kwargs)
        self.run_assertion(("SecondPass"+name+".png"), ("second pass "+name), failures)
            
    def test_FootPrintNodeForSecondPass(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            #Create a mayaHydraFootPrintNode node which adds a dataProducerSceneIndex
            footPrintNodeName = cmds.createNode("MhFootPrint")
            self.setBasicCam(1)
            #Clear selection
            cmds.select(clear=True)
            
            failures = []
            
            self.EnablePassesOneByOneAndDoSnapshots(name="FootPrintAsGeom", failures=failures)
            
            self.SetAllPassesVisibleToColorAOV()
            self.run_assertion(filename="asGeom_AllPasses.png", mode="all passes", failures=failures)

            #Draw the foot print node as a secondary graphics 
            #It should make the node be part of the secondary graphics pass if there is one, 
            #as passes can be merged.
            cmds.setAttr(footPrintNodeName + '.drawAsSecondaryGraphics', True)
            cmds.refresh()

            self.setBasicCam(1)
            self.EnablePassesOneByOneAndDoSnapshots(name="FootPrintAsScndGraphics", failures=failures)

            # Enable all passes
            self.SetAllPassesVisibleToColorAOV()
            self.run_assertion(filename="asScndGraphics_AllPasses.png", mode="ScndGraphics all passes", failures=failures)
            
            # After all tests have run, report any failures
            if failures:
                self.fail(f"The following assertions failed:\n" + "\n".join(failures))

    def test_FramePassesAllData(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            
            failures = []
            
            # open a Maya scene with all data sources (maya, usd, custom)
            testFile = mayaUtils.openTestScene(
                    "testFramePasses",
                    "framePasses.ma", useTestSettings=False)
            cmds.refresh()
            self.run_assertion("sceneLoaded.png", "scene loaded", failures)
            #Render all passes
            self.EnablePassesOneByOneAndDoSnapshots(name="SceneLoaded", failures=failures)
            #Render all passes using the "depth" AOV
            self.EnablePassesOneByOneAndDoSnapshots(name="SceneLoadedDepth", failures=failures, aovName="depth")
            
            #Draw the foot print node as secondary graphics 
            #It should make the node be part of the secondary graphics pass if there is one, 
            #as passes can be merged.
            cmds.setAttr('MhFootPrint1.drawAsSecondaryGraphics', True)
            cmds.refresh()

            #Render all passes
            self.EnablePassesOneByOneAndDoSnapshots(name="SceneLoadedFPScndGraphics", failures=failures)

            # Enable all passes
            self.SetAllPassesVisibleToColorAOV()

            #Select all meshes, the sel highlight prims should appear in the 2nd pass only
            cmds.select(clear=True) #maya
            cmds.select('|pSphere1', add=True) #maya
            cmds.select('|transform1', add=True) #custom prim FootStep
            cmds.select('|PoolBallFlat_animated|PoolBallFlat_animatedShape', add=True)#usd prim

            self.EnablePassesOneByOneAndDoSnapshots(name="SelHighlight", failures=failures)
            
            #Set a wrong aov Name on purpose, it should show the color aov by default
            cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=[0, 1], aovName="wrongAovName1")
            self.run_assertion("wrongAovName.png", "Wrong AOV name", failures)

            # Enable all passes
            self.SetAllPassesVisibleToColorAOV()
            
            # After all tests have run, report any failures
            if failures:
                self.fail(f"The following assertions failed:\n" + "\n".join(failures))
    
    def test_ListAovs(self):
        stormAovs = cmds.mayaHydraSetVisibleFramePasses(listAovs=0)
        if self._usdVersion <= (0, 24, 11):
            self.assertEqual(stormAovs, ['color', 'primId', 'depth'])
        else:
            self.assertEqual(stormAovs, ['color', 'primId', 'depth', 'Neye'])
    
    def test_Reset(self):
        with PluginLoaded('mayaHydraFootPrintNode'):
            mayaUtils.openTestScene(
                        "testFramePasses",
                        "framePasses.ma", useTestSettings=False)
            cmds.refresh()
            cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=[0], aovName="depth")
            self.assertSnapshotClose("beforeReset.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)
            cmds.mayaHydraSetVisibleFramePasses(reset=True)
            self.assertSnapshotClose("afterReset.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
