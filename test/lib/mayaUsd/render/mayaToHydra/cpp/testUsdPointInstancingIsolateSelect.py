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
import maya.cmds as cmds
import maya.mel as mel
import usdUtils
from pxr import UsdGeom
import testUtils

def _setTranslate(prim, value):
    # setupScene() runs once per test method but Maya/USD are shared across
    # tests in the same class. USD's stage cache can keep the layer in
    # memory between tests, so a previously-added translate op may still be
    # present on the prim. AddTranslateOp() throws in that case (observed
    # only in Coverage builds where object lifetimes differ).
    xf = UsdGeom.Xformable(prim)
    for op in xf.GetOrderedXformOps():
        if op.GetOpType() == UsdGeom.XformOp.TypeTranslate:
            op.Set(value)
            return
    xf.AddTranslateOp().Set(value)

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

# This test is identical to the one in testIsolateSelect.py, except for
# disabled tests.  See HYDRA-1245.

class TestUsdPointInstancingIsolateSelect(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    _requiredPlugins = ['mayaHydraCppTests', 'mayaHydraFlowViewportAPILocator']

    def setupScene(self):

        # Read in a scene with native instancing.
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', 'instancedCubeHierarchies.usda')

        self.proxyShapePathStr = usdUtils.createStageFromFile(usdScenePath)

        stage = mayaUsd.lib.GetPrim(self.proxyShapePathStr).GetStage()

        # Add in non-instanced USD prims.
        stage.DefinePrim('/parent1', 'Xform')
        stage.DefinePrim('/parent2', 'Xform')

        cylinder1 = UsdGeom.Cylinder.Define(stage, "/parent1/cylinder1")
        cone1 = UsdGeom.Cone.Define(stage, "/parent2/cone1")
        sphere1 = UsdGeom.Sphere.Define(stage, "/parent2/sphere1")

        # Add in a point instancer and its point instances.
        ref = stage.DefinePrim('/ref')
        piScenePath = testUtils.getTestScene('testUsdPointInstances', 'pointInstancer.usda')
        ref.GetReferences().AddReference(str(piScenePath))
        self.pointInstancerPath = self.proxyShapePathStr + ',/ref/CubePointInstancer'

        # Move objects around so that snapshot comparison is significant.

        # Move USD objects.  Can also use undoable Maya cmds.move(), but using
        # the USD APIs is simpler.
        _setTranslate(cylinder1.GetPrim(), (0, 2, 0))
        _setTranslate(cone1.GetPrim(), (2, 0, 0))
        _setTranslate(sphere1.GetPrim(), (-2, 0, 0))
        _setTranslate(ref, (0, 0, -2))

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
        self.cubeGenPath = '|transform1|MhFlowViewportAPILocator1'
        cmds.setAttr("MhFlowViewportAPILocator1.numCubesX", 2)
        cmds.setAttr("MhFlowViewportAPILocator1.numCubesY", 2)
        cmds.setAttr("MhFlowViewportAPILocator1.cubeHalfSize", 0.25)
        cmds.setAttr("MhFlowViewportAPILocator1.cubesDeltaTrans", 1, 1, 1)

        cmds.move(0, 0, 4, "transform1", r=True)

        cmds.refresh()

        scene = [
            # Maya objects
            '|pTorus1', 
            '|pTorus1|pTorusShape1', 
            '|group1',
            '|group1|pSphere1',
            '|group1|pSphere1|pSphereShape1',
            '|group1|pCube1',
            '|group1|pCube1|pCubeShape1',
            '|group2',
            '|group2|pCone1',
            '|group2|pCone1|pConeShape1',
            # Non-instanced USD objects
            self.proxyShapePathStr + ',/parent1',
            self.proxyShapePathStr + ',/parent1/cylinder1',
            self.proxyShapePathStr + ',/parent2',
            self.proxyShapePathStr + ',/parent2/cone1',
            self.proxyShapePathStr + ',/parent2/sphere1',
            # Native instanced USD objects
            self.proxyShapePathStr + ',/cubeHierarchies/cubes_1',
            self.proxyShapePathStr + ',/cubeHierarchies/cubes_2',
            # USD point instancer
            self.pointInstancerPath,
            # Flow Viewport API procedural cube generator.
            self.cubeGenPath]

        # Cube generator cubes
        for i in range(0, 2):
            for j in range(0, 2):
                scene.append(self.cubeGenPath + (',/cube_%s_%s_0' % (i, j)))

        # Point instances
        for i in range(0, 14):
            scene.append(self.pointInstancerPath + ('/%s' % i))

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

    def assertIsolateSelect(self, modelPanel, visible, scene):
        cmds.select(*visible)
        cmds.editor(modelPanel, edit=True, updateMainConnection=True)
        cmds.isolateSelect(modelPanel, loadSelected=True)

        notVisible = scene.copy()

        for p in visible:
            notVisible.remove(p)

        cmds.refresh()

        self.assertVisibility(visible, notVisible)

    def test_isolateSelectPointInstancing(self):

        scene = self.setupScene()

        cmds.select(clear=True)

        # Isolate select not turned on, everything visible.
        visible = scene
        notVisible = []

        cmds.refresh()

        self.assertVisibility(visible, notVisible)

        # Turn isolate select on, nothing selected, nothing visible.
        visible = []
        notVisible = scene

        modelPanel = 'modelPanel4'
        enableIsolateSelect(modelPanel)
        
        cmds.refresh()

        self.assertVisibility(visible, notVisible)

        # Select various point instances.  The instancer itself will be visible,
        # everything else not visible.
        visible = [
            self.pointInstancerPath,        self.pointInstancerPath + '/0',
            self.pointInstancerPath + '/3', self.pointInstancerPath + '/12']
        self.assertIsolateSelect(modelPanel, visible, scene)

        # Add in some Maya objects, a non-instanced USD object, a native
        # instanced USD object, and a generated cube.
        visible = [
            # Point instanced USD objects
            self.pointInstancerPath,        self.pointInstancerPath + '/0',
            self.pointInstancerPath + '/3', self.pointInstancerPath + '/12',
            # Maya objects
            '|pTorus1', '|pTorus1|pTorusShape1', '|group2', '|group2|pCone1',
            '|group2|pCone1|pConeShape1',
            # Non-instanced USD object
            self.proxyShapePathStr + ',/parent1',
            self.proxyShapePathStr + ',/parent1/cylinder1',
            # Native instanced USD object
            self.proxyShapePathStr + ',/cubeHierarchies/cubes_1',
            # Generated cube
            self.cubeGenPath,
            self.cubeGenPath + ',/cube_0_0_0']

        self.assertIsolateSelect(modelPanel, visible, scene)

        disableIsolateSelect(modelPanel)

    def test_isolateSelectPointInstancingPreSelected(self):
        """Regression test for the bug where selecting multiple point instances
        and then enabling isolate select (rather than using loadSelected=True
        after enabling) caused the viewport to freeze.

        The root cause was that Maya stores multiple selected instances of the
        same PointInstancer as a single view-selected object whose
        objectStrings array has one entry per instance (length > 1).  The old
        code unconditionally skipped any entry with objectStrings.length() > 1,
        producing an empty isolate selection.  An empty selection triggers
        dirtying of every prim in the scene, which causes an apparent freeze
        while Storm processes the massive dirty list.
        """
        scene = self.setupScene()

        modelPanel = 'modelPanel4'

        # Pre-select 2 point instances (without the instancer itself), then
        # enable isolate select.  This is the workflow that triggered the bug:
        # the instances ended up in a single component-style view-selected
        # entry (objectStrings.length() > 1) and were silently dropped.
        cmds.select(self.pointInstancerPath + '/1',
                    self.pointInstancerPath + '/5')
        enableIsolateSelect(modelPanel)

        # Only the instancer prim and the two explicitly selected instances
        # should be visible.  All Maya objects, non-instanced USD prims,
        # native instances, the cube generator, and every other point instance
        # (0, 2, 3, 4, 6-13) must be hidden.
        visible = [
            self.pointInstancerPath,
            self.pointInstancerPath + '/1',
            self.pointInstancerPath + '/5']
        notVisible = scene.copy()
        for p in visible:
            notVisible.remove(p)

        cmds.refresh()

        self.assertVisibility(visible, notVisible)

        disableIsolateSelect(modelPanel)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
