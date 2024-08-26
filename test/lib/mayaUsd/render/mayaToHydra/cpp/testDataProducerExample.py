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
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
import ufe

class TestDataProducerExample(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _pluginsToLoad = ['mayaHydraCppTests', 'mayaHydraFlowViewportAPILocator']
    _pluginsToUnload = []

    def createScene(self):
        self._locator = cmds.createNode('MhFlowViewportAPILocator')
        cmds.setAttr(self._locator + '.numCubesX', 3)
        cmds.setAttr(self._locator + '.numCubesY', 3)
        cmds.setAttr(self._locator + '.numCubesZ', 3)

    @classmethod
    def setUpClass(cls):
        super(TestDataProducerExample, cls).setUpClass()
        for p in cls._pluginsToLoad:
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

    @classmethod
    def tearDownClass(cls):
        super(TestDataProducerExample, cls).tearDownClass()
        # Clean out the scene to allow all plugins to unload cleanly.
        cmds.file(new=True, force=True)
        for p in reversed(cls._pluginsToUnload):
            if p != 'mayaHydraFlowViewportAPILocator':
                cmds.unloadPlugin(p)

    def setUp(self):
        super(TestDataProducerExample, self).setUp()
        self.createScene()
        cmds.refresh()

    def cube000PathString(self):
        return '|transform1|' + self._locator + ',/cube_0_0_0'

    def cube222PathString(self):
        return '|transform1|' + self._locator + ',/cube_2_2_2'

    def cubePrototypePathString(self):
        return '|transform1|' + self._locator + ',/cube_'

    def test_Pick(self):
        # Pick an exterior cube to ensure we don't pick a hidden one.
        cmds.mayaHydraCppTest(self.cube222PathString(), f='TestUsdPicking.pick')

    def test_Select(self):
        # Make a selection, ensure the Hydra selection changes.
        sn = ufe.GlobalSelection.get()
        sn.clear()

        # Empty Maya selection, therefore no fully selected path in the scene
        # index.
        cmds.mayaHydraCppTest(f='TestSelection.fullySelectedPaths')

        item = ufe.Hierarchy.createItem(ufe.PathString.path(self.cube222PathString()))
        sn.append(item)

        # Item added to the Maya selection, it should be fully selected in the
        # scene index.
        cmds.mayaHydraCppTest(
            self.cube222PathString(), f='TestSelection.fullySelectedPaths')

    def test_Hide(self):
        # Select a cube, hide it, demonstrate it's hidden.
        sn = ufe.GlobalSelection.get()
        sn.clear()
        item = ufe.Hierarchy.createItem(
            ufe.PathString.path(self.cube222PathString()))
        sn.append(item)

        # Cube is not hidden yet, and is present in the scene index.
        o3d = ufe.Object3d.object3d(item)
        self.assertTrue(o3d.visibility())
        cmds.mayaHydraCppTest(self.cube222PathString(), f='TestHydraPrim.isFound')

        # Hide it, undo, redo.
        cmds.hide()
        self.assertFalse(o3d.visibility())
        cmds.mayaHydraCppTest(self.cube222PathString(), f='TestHydraPrim.isNotFound')
        cmds.undo()
        self.assertTrue(o3d.visibility())
        cmds.mayaHydraCppTest(self.cube222PathString(), f='TestHydraPrim.isFound')
        cmds.redo()
        self.assertFalse(o3d.visibility())
        cmds.mayaHydraCppTest(self.cube222PathString(), f='TestHydraPrim.isNotFound')

    def test_Move(self):
        # Select a cube, move it, demonstrate it's moved.  Use the cube closest
        # to the parent origin, to avoid dealing with inter-cube spacing and
        # cube indexing.
        sn = ufe.GlobalSelection.get()
        sn.clear()
        item = ufe.Hierarchy.createItem(
            ufe.PathString.path(self.cube000PathString()))
        sn.append(item)

        # Cube hasn't moved yet.
        t3d = ufe.Transform3d.transform3d(item)

        def assertTranslationAlmostEqual(expected):
            self.assertEqual(t3d.translation().vector, expected)
            cmds.mayaHydraCppTest(self.cube000PathString(), str(expected[0]),
                                  str(expected[1]), str(expected[2]), 
                                  f='TestHydraPrim.translation')

        assertTranslationAlmostEqual([0, 0, 0])

        # Move it, undo, redo.
        cmds.move(3, 4, 5)
        assertTranslationAlmostEqual([3, 4, 5])
        cmds.undo()
        assertTranslationAlmostEqual([0, 0, 0])
        cmds.redo()
        assertTranslationAlmostEqual([3, 4, 5])

    def test_SelectPrototype(self):
        # Enable instancing
        cmds.setAttr(self._locator + '.cubesUseInstancing', True)

        # Clear selection
        sn = ufe.GlobalSelection.get()
        sn.clear()

        # Empty Maya selection, therefore no fully selected path in the scene
        # index.
        cmds.mayaHydraCppTest(f='TestSelection.fullySelectedPaths')

        # Add cube to selection
        item = ufe.Hierarchy.createItem(ufe.PathString.path(self.cubePrototypePathString()))
        sn.append(item)
        
        # Item added to the Maya selection, it should be fully selected in the
        # scene index.
        cmds.mayaHydraCppTest(
            self.cubePrototypePathString(), f='TestSelection.fullySelectedPaths')

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
