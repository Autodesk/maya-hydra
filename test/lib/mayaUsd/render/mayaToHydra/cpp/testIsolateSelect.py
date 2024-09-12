# Copyright 2024 Autodesk
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

import fixturesUtils
import mtohUtils
import mayaUsd
import mayaUsd_createStageWithNewLayer
import maya.cmds as cmds
import maya.mel as mel
from pxr import UsdGeom

def enableIsolateSelect(modelPanel):
    # Surprisingly
    # 
    # cmds.isolateSelect('modelPanel1', state=1)
    # 
    # is insufficient to turn on isolate selection in a viewport, and we must
    # use the MEL script used by the menu and Ctrl-1 hotkey.  This is because
    # the viewport uses the selectionConnection command to filter the selection
    # it receives and create its isolate selection, and the the
    # mainListConnection, lockMainConnection and unlockMainConnection flags of
    # the editor command to suspend changes to its selection connection.  See
    # the documentation for more details.
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 1" % modelPanel)
    
def disableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 0" % modelPanel)

class TestIsolateSelect(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    _pluginsToLoad = ['mayaHydraCppTests', 'mayaHydraFlowViewportAPILocator']
    _pluginsToUnload = []

    @classmethod
    def setUpClass(cls):
        super(TestIsolateSelect, cls).setUpClass()
        for p in cls._pluginsToLoad:
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

    @classmethod
    def tearDownClass(cls):
        super(TestIsolateSelect, cls).tearDownClass()
        # Clean out the scene to allow all plugins to unload cleanly.
        cmds.file(new=True, force=True)
        for p in reversed(cls._pluginsToUnload):
            if p != 'mayaHydraFlowViewportAPILocator':
                cmds.unloadPlugin(p)

    def setupScene(self):
        proxyShapePathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(proxyShapePathStr).GetStage()

        stage.DefinePrim('/parent1', 'Xform')
        stage.DefinePrim('/parent2', 'Xform')
        
        cylinder1 = UsdGeom.Cylinder.Define(stage, "/parent1/cylinder1")
        cone1 = UsdGeom.Cone.Define(stage, "/parent2/cone1")
        sphere1 = UsdGeom.Sphere.Define(stage, "/parent2/sphere1")

        # Move objects around so that snapshot comparison is significant.

        # Move USD objects.  Can also use undoable Maya cmds.move(), but using
        # the USD APIs is simpler.
        cylinder1.AddTranslateOp().Set(value=(0, 2, 0))
        cone1.AddTranslateOp().Set(value=(2, 0, 0))
        sphere1.AddTranslateOp().Set(value=(-2, 0, 0))

        cmds.polyTorus()
        cmds.polySphere()
        cmds.move(2, 0, 0, r=True)
        cmds.polyCube()
        cmds.move(-2, 0, 0, r=True)
        cmds.polyCone()
        cmds.group('pSphere1', 'pCube1')
        cmds.move(0, 0, 2, r=True)
        cmds.group('pCone1')
        cmds.move(0, 0, 2, r=True)

        # Add a Hydra-only data producer.
        cmds.createNode("MhFlowViewportAPILocator")
        cmds.setAttr("MhFlowViewportAPILocator1.numCubesX", 2)
        cmds.setAttr("MhFlowViewportAPILocator1.numCubesY", 2)
        cmds.setAttr("MhFlowViewportAPILocator1.cubeHalfSize", 0.25)
        cmds.setAttr("MhFlowViewportAPILocator1.cubesDeltaTrans", 1, 1, 1)

        cmds.move(0, 0, 4, "transform1", r=True)

        cmds.refresh()

        return  ['|pTorus1', 
                 '|pTorus1|pTorusShape1', 
                 '|stage1|stageShape1,/parent1',
                 '|stage1|stageShape1,/parent1/cylinder1',
                 '|stage1|stageShape1,/parent2',
                 '|stage1|stageShape1,/parent2/cone1',
                 '|stage1|stageShape1,/parent2/sphere1',
                 '|group1',
                 '|group1|pSphere1',
                 '|group1|pSphere1|pSphereShape1',
                 '|group1|pCube1',
                 '|group1|pCube1|pCubeShape1',
                 '|group2',
                 '|group2|pCone1',
                 '|group2|pCone1|pConeShape1']

    def setupMultiStageScene(self):
        scene = self.setupScene()

        proxyShapePathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(proxyShapePathStr).GetStage()

        stage.DefinePrim('/parent1', 'Xform')
        stage.DefinePrim('/parent2', 'Xform')
        
        UsdGeom.Cylinder.Define(stage, "/parent1/cylinder1")
        UsdGeom.Cone.Define(stage, "/parent2/cone1")
        UsdGeom.Sphere.Define(stage, "/parent2/sphere1")

        scene.extend([
            '|stage2|stageShape2,/parent1',
            '|stage2|stageShape2,/parent1/cylinder1',
            '|stage2|stageShape2,/parent2',
            '|stage2|stageShape2,/parent2/cone1',
            '|stage2|stageShape2,/parent2/sphere1'])
        return scene

    def assertVisible(self, visible):
        for v in visible:
            self.trace("Testing %s for visibility\n" % v)
            cmds.mayaHydraCppTest(v, f="TestHydraPrim.isVisible")

    def assertNotVisible(self, notVisible):
        for nv in notVisible:
            self.trace("Testing %s for invisibility\n" % nv)
            cmds.mayaHydraCppTest(nv, f="TestHydraPrim.notVisible")

    def assertVisibility(self, visible, notVisible):
        self.assertVisible(visible)
        self.assertNotVisible(notVisible)

    def test_isolateSelectSingleViewport(self):
        scene = self.setupScene()

        # The default viewport is in the following panel.
        modelPanel = 'modelPanel4'

        cmds.select(clear=True)
        enableIsolateSelect(modelPanel)

        #============================================================
        # Add
        #============================================================

        # Add a single object to the isolate selection.  Only that object,
        # its ancestors, and its descendants are visible.
        cmds.mayaHydraCppTest(modelPanel, "|pTorus1", f="TestIsolateSelection.add")

        visible = ['|pTorus1', '|pTorus1|pTorusShape1']
        notVisible = scene.copy()
        for v in visible:
            notVisible.remove(v)

        self.assertVisibility(visible, notVisible)

        # Add a USD object to the isolate selection.
        cmds.mayaHydraCppTest(modelPanel, '|stage1|stageShape1,/parent2', f="TestIsolateSelection.add")

        for p in ['|stage1|stageShape1,/parent2',
                  '|stage1|stageShape1,/parent2/cone1',
                  '|stage1|stageShape1,/parent2/sphere1']:
            visible.append(p)
            notVisible.remove(p)

        self.assertVisibility(visible, notVisible)

        #============================================================
        # Remove
        #============================================================

        # Remove the Maya object from the isolate selection.  Only the USD
        # isolate selected objects are visible.
        cmds.mayaHydraCppTest(modelPanel, "|pTorus1", f="TestIsolateSelection.remove")

        visible.clear()
        notVisible = scene.copy()

        for p in ['|stage1|stageShape1,/parent2',
                  '|stage1|stageShape1,/parent2/cone1',
                  '|stage1|stageShape1,/parent2/sphere1']:
            visible.append(p)
            notVisible.remove(p)

        self.assertVisibility(visible, notVisible)

        # Remove the USD isolate selected object.  The isolate selection
        # is empty, isolate selection is enabled, so nothing is now visible.
        cmds.mayaHydraCppTest(modelPanel, '|stage1|stageShape1,/parent2', f="TestIsolateSelection.remove")

        visible.clear()
        notVisible = scene.copy()

        self.assertVisibility(visible, notVisible)

        #============================================================
        # Clear
        #============================================================

        # Add an object back to the isolate selection.
        cmds.mayaHydraCppTest(modelPanel, '|stage1|stageShape1,/parent1/cylinder1', f="TestIsolateSelection.add")

        notVisible = scene.copy()
        visible.clear()

        for p in ['|stage1|stageShape1,/parent1',
                  '|stage1|stageShape1,/parent1/cylinder1']:
            visible.append(p)
            notVisible.remove(p)

        self.assertVisibility(visible, notVisible)

        # Clear the isolate selection.
        cmds.mayaHydraCppTest(modelPanel, f="TestIsolateSelection.clear")

        visible.clear()
        notVisible = scene.copy()

        self.assertVisibility(visible, notVisible)

        #============================================================
        # Replace
        #============================================================

        # Add an object back to the isolate selection.
        cmds.mayaHydraCppTest(modelPanel, '|group2|pCone1', f="TestIsolateSelection.add")

        notVisible = scene.copy()
        visible.clear()

        for p in ['|group2', '|group2|pCone1', '|group2|pCone1|pConeShape1']:
            visible.append(p)
            notVisible.remove(p)

        self.assertVisibility(visible, notVisible)

        # Replace this isolate selection with a different one.
        cmds.mayaHydraCppTest(modelPanel, '|group1|pCube1', '|stage1|stageShape1,/parent2/cone1', f="TestIsolateSelection.replace")

        visible.clear()
        notVisible = scene.copy()

        for p in ['|group1', '|group1|pCube1', '|group1|pCube1|pCubeShape1',
                  '|stage1|stageShape1,/parent2',
                  '|stage1|stageShape1,/parent2/cone1']:
            visible.append(p)
            notVisible.remove(p)

        self.assertVisibility(visible, notVisible)

        # Disable the isolate selection to avoid affecting other tests.
        disableIsolateSelect(modelPanel)

    # A robust multi-viewport test would have tested scene index prim
    # visibility for each viewport, and demonstrated per-viewport visibility.
    # Unfortunately, tracing demonstrates that we can't obtain scene index prim
    # visibility in one viewport before a draw is performed in another
    # viewport.  Since visibility is according to the last viewport drawn, and
    # don't know of way to control order of viewport draw, this testing
    # strategy fails.
    #
    # Unfortunately performing image comparisons is also not possible, as at
    # time of writing playblast doesn't respect isolate select when using
    # MayaHydra in a multi-viewport setting.  Therefore, the following test is
    # weak and does not validate results.
    def test_isolateSelectMultiViewport(self):
        scene = self.setupScene()
            
        # We start in single viewport mode.  Set an isolate selection there.
        cmds.select('|group1', '|stage1|stageShape1,/parent1/cylinder1')
        enableIsolateSelect("modelPanel4")

        notVisible = scene.copy()
        visible = ['|group1',
                   '|group1|pSphere1',
                   '|group1|pSphere1|pSphereShape1',
                   '|group1|pCube1',
                   '|group1|pCube1|pCubeShape1',
                   '|stage1|stageShape1,/parent1',
                   '|stage1|stageShape1,/parent1/cylinder1']
        for p in visible:
            notVisible.remove(p)

        cmds.refresh()

        self.assertVisibility(visible, notVisible)

        # Move the camera closer for a view where objects fill in more of the
        # viewport. FrameSelectedWithoutChildren() is good, but a manually
        # chosen camera position is better.
        cmds.setAttr("persp.translate", 4.9, 3.5, 5.7)
        cmds.select(clear=1)

        cmds.refresh()

        self.assertSnapshotClose("singleViewportIsolateSelectCylinder1.png", 0.1, 2)

        # Switch to four-up viewport mode.  Set the renderer in each new
        # viewport to be Hydra Storm.  Viewport 4 is already set.
        cmds.FourViewLayout()
        visible = scene.copy()
        notVisible.clear()
        for i in range(1, 4):
            cmds.setFocus('modelPanel'+str(i))
            self.setHdStormRenderer()

        cmds.select('|pTorus1')

        enableIsolateSelect("modelPanel1")

        cmds.refresh()

        # As a final step disable the isolate selections to avoid affecting
        # other tests.
        for i in range(1, 5):
            disableIsolateSelect('modelPanel'+str(i))

    def test_isolateSelectMultipleStages(self):
        scene = self.setupMultiStageScene()

        modelPanel = 'modelPanel4'

        cmds.select('|group1|pCube1', '|stage1|stageShape1,/parent2/cone1',
            '|stage2|stageShape2,/parent1/cylinder1')
        enableIsolateSelect(modelPanel)

        visible = ['|group1', '|group1|pCube1', '|group1|pCube1|pCubeShape1',
                   '|stage1|stageShape1,/parent2',
                   '|stage1|stageShape1,/parent2/cone1',
                   '|stage2|stageShape2,/parent1',
                   '|stage2|stageShape2,/parent1/cylinder1']
        notVisible = scene.copy()

        for p in visible:
            notVisible.remove(p)

        cmds.refresh()

        self.assertVisibility(visible, notVisible)

        # As a final step disable the isolate selection to avoid affecting
        # other tests.
        disableIsolateSelect(modelPanel)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
