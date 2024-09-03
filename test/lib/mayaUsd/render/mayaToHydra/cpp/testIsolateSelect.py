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
from testUtils import PluginLoaded
import mayaUsd
import mayaUsd_createStageWithNewLayer
import maya.cmds as cmds
from pxr import UsdGeom

class TestIsolateSelect(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    def setupScene(self):
        proxyShapePathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(proxyShapePathStr).GetStage()

        stage.DefinePrim('/parent1', 'Xform')
        stage.DefinePrim('/parent2', 'Xform')
        
        UsdGeom.Cylinder.Define(stage, "/parent1/cylinder1")
        UsdGeom.Cone.Define(stage, "/parent2/cone1")
        UsdGeom.Sphere.Define(stage, "/parent2/sphere1")

        cmds.polyTorus()
        cmds.polySphere()
        cmds.polyCube()
        cmds.polyCone()
        cmds.group('pSphere1', 'pCube1')
        cmds.group('pCone1')

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
        with PluginLoaded('mayaHydraCppTests'):
            # The default viewport is in the following panel.
            vpPanel = 'modelPanel4'

            #============================================================
            # Add
            #============================================================

            # Add a single object to the isolate selection.  Only that object,
            # its ancestors, and its descendants are visible.
            cmds.mayaHydraCppTest(vpPanel, "|pTorus1", f="TestIsolateSelection.add")

            visible = ['|pTorus1', '|pTorus1|pTorusShape1']
            notVisible = scene.copy()
            for v in visible:
                notVisible.remove(v)

            self.assertVisibility(visible, notVisible)

            # Add a USD object to the isolate selection.
            cmds.mayaHydraCppTest(vpPanel, '|stage1|stageShape1,/parent2', f="TestIsolateSelection.add")

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
            cmds.mayaHydraCppTest(vpPanel, "|pTorus1", f="TestIsolateSelection.remove")

            visible.clear()
            notVisible = scene.copy()

            for p in ['|stage1|stageShape1,/parent2',
                      '|stage1|stageShape1,/parent2/cone1',
                      '|stage1|stageShape1,/parent2/sphere1']:
                visible.append(p)
                notVisible.remove(p)

            self.assertVisibility(visible, notVisible)

            # Remove the USD isolate selected object.  Everything is now visible.
            cmds.mayaHydraCppTest(vpPanel, '|stage1|stageShape1,/parent2', f="TestIsolateSelection.remove")

            visible = scene.copy()
            notVisible.clear()

            self.assertVisibility(visible, notVisible)

            #============================================================
            # Clear
            #============================================================

            # Add an object back to the isolate selection.
            cmds.mayaHydraCppTest(vpPanel, '|stage1|stageShape1,/parent1/cylinder1', f="TestIsolateSelection.add")

            notVisible = scene.copy()
            visible.clear()

            for p in ['|stage1|stageShape1,/parent1',
                      '|stage1|stageShape1,/parent1/cylinder1']:
                visible.append(p)
                notVisible.remove(p)

            self.assertVisibility(visible, notVisible)

            # Clear the isolate selection.
            cmds.mayaHydraCppTest(vpPanel, f="TestIsolateSelection.clear")

            visible = scene.copy()
            notVisible.clear()

            self.assertVisibility(visible, notVisible)

            #============================================================
            # Replace
            #============================================================

            # Add an object back to the isolate selection.
            cmds.mayaHydraCppTest(vpPanel, '|group2|pCone1', f="TestIsolateSelection.add")

            notVisible = scene.copy()
            visible.clear()

            for p in ['|group2', '|group2|pCone1', '|group2|pCone1|pConeShape1']:
                visible.append(p)
                notVisible.remove(p)

            self.assertVisibility(visible, notVisible)

            # Replace this isolate selection with a different one.
            cmds.mayaHydraCppTest(vpPanel, '|group1|pCube1', '|stage1|stageShape1,/parent2/cone1', f="TestIsolateSelection.replace")

            visible.clear()
            notVisible = scene.copy()

            for p in ['|group1', '|group1|pCube1', '|group1|pCube1|pCubeShape1',
                      '|stage1|stageShape1,/parent2',
                      '|stage1|stageShape1,/parent2/cone1']:
                visible.append(p)
                notVisible.remove(p)

            self.assertVisibility(visible, notVisible)

            # Clear the isolate selection to avoid affecting other tests.
            cmds.mayaHydraCppTest(vpPanel, f="TestIsolateSelection.clear")

    # To test multi-viewport behavior we would have wanted to test scene index
    # prim visibility for each viewport, and demonstrate per-viewport
    # visibility.  Unfortunately, tracing demonstrates that we can't obtain
    # scene index prim visibility in one viewport before a draw is performed in
    # another viewport.
    #
    # Since visibility is according to the last viewport drawn, and don't know
    # of way to control order of viewport draw, this testing strategy fails.
    # For example, consider drawing modelPanel4, then modelPanel1:
    # 
    #======================================================================
    # Re-using existing scene index chain to render modelPanel4
    # found isolate selection 0000022DD5557B30 for viewport ID modelPanel4
    # Re-using isolate selection 0000022DD5557B30
    # Rendering destination is modelPanel1
    # Re-using existing scene index chain to render modelPanel1
    # found isolate selection 0000022E05EC5740 for viewport ID modelPanel1
    # Switching scene index to isolate selection 0000022E05EC5740
    # IsolateSelectSceneIndex::SetViewport() called for new viewport modelPanel1.
    #     Old viewport was modelPanel4.
    #     Old selection is 0000022DD5557B30, new selection is 0000022E05EC5740.
    #     modelPanel4: examining /MayaHydraViewportRenderer/rprims/Lighted/pSphere1 for isolate select dirtying.
    # [...]
    # [Dirtying to bring objects invisible in modelPanel4 into modelPanel1]
    # [...]
    # [Multiple GetPrim() calls for modelPanel1 which all succeed because the
    # isolate selection for modelPanel1 is empty]
    # IsolateSelectSceneIndex::GetPrim(/MayaHydraViewportRenderer/rprims/pCone1/pConeShape1/DormantPolyWire_58) called for viewport modelPanel1.
    # [...]
    # Rendering destination is modelPanel1
    # Re-using existing scene index chain to render modelPanel1
    # found isolate selection 0000022E05EC5740 for viewport ID modelPanel1
    # Re-using isolate selection 0000022E05EC5740
    #
    # [For an unknown reason we switch back to rendering modelPanel4.]
    # 
    # Rendering destination is modelPanel4
    # Re-using existing scene index chain to render modelPanel4
    # found isolate selection 0000022DD5557B30 for viewport ID modelPanel4
    # Switching scene index to isolate selection 0000022DD5557B30
    # IsolateSelectSceneIndex::SetViewport() called for new viewport modelPanel4.
    #     Old viewport was modelPanel1.
    #     Old selection is 0000022E05EC5740, new selection is 0000022DD5557B30.
    #======================================================================
    # 
    # And at this point if we ask for visibility we'll get the modelPanel4
    # visibility, rather than the desired modelPanel1 visibility.
    #
    # We may have to resort to image comparison to test this.

    def test_isolateSelectMultiViewport(self):
        scene = self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            
            # We start in single viewport mode.  Set an isolate selection there.
            cmds.mayaHydraCppTest('modelPanel4', '|group1', '|stage1|stageShape1,/parent1/cylinder1', f="TestIsolateSelection.replace")

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

            self.assertVisibility(visible, notVisible)

            # Switch to four-up viewport mode.  Set the renderer in each new
            # viewport to be Hydra Storm.  Viewport 4 is already set.
            # Everything should be initially visible in viewports 1-3.
            cmds.FourViewLayout()
            visible = scene.copy()
            notVisible.clear()
            for i in range(1, 4):
                cmds.setFocus('modelPanel'+str(i))
                self.setHdStormRenderer()
                # self.assertVisibility(visible, notVisible)

            # Here we would set different isolate selections in each viewport.

            # As a final step clear the isolate selections to avoid affecting
            # other tests.
            for i in range(1, 5):
                modelPanel = 'modelPanel'+str(i)
                cmds.setFocus(modelPanel)
                cmds.mayaHydraCppTest(modelPanel, f="TestIsolateSelection.clear")

    def test_isolateSelectMultipleStages(self):
        scene = self.setupMultiStageScene()
        with PluginLoaded('mayaHydraCppTests'):
            vpPanel = 'modelPanel4'
            cmds.mayaHydraCppTest(
                vpPanel, '|group1|pCube1', '|stage1|stageShape1,/parent2/cone1',
                '|stage2|stageShape2,/parent1/cylinder1',
                f="TestIsolateSelection.replace")
            
            visible = ['|group1', '|group1|pCube1', '|group1|pCube1|pCubeShape1',
                       '|stage1|stageShape1,/parent2',
                       '|stage1|stageShape1,/parent2/cone1',
                       '|stage2|stageShape2,/parent1',
                       '|stage2|stageShape2,/parent1/cylinder1']
            notVisible = scene.copy()

            for p in visible:
                notVisible.remove(p)

            self.assertVisibility(visible, notVisible)

            # As a final step clear the isolate selection to avoid affecting
            # other tests.
            cmds.mayaHydraCppTest(vpPanel, f="TestIsolateSelection.clear")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
