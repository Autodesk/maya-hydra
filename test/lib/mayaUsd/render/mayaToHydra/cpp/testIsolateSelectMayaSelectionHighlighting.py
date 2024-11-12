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
import maya.cmds as cmds
import maya.mel as mel

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

# This test is very similar to testIsolateSelect.py.  See HYDRA-1245.

class TestIsolateSelectMayaSelectionHighlighting(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    _pluginsToLoad = ['mayaHydraCppTests']
    _pluginsToUnload = []

    @classmethod
    def setUpClass(cls):
        super(TestIsolateSelectMayaSelectionHighlighting, cls).setUpClass()
        for p in cls._pluginsToLoad:
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

    @classmethod
    def tearDownClass(cls):
        super(TestIsolateSelectMayaSelectionHighlighting, cls).tearDownClass()
        # Clean out the scene to allow all plugins to unload cleanly.
        cmds.file(new=True, force=True)
        for p in reversed(cls._pluginsToUnload):
            cmds.unloadPlugin(p)

    def setupScene(self):

        cmds.polyTorus()
        cmds.polySphere()
        cmds.move(2, 0, 0, r=True)
        cmds.polyCube()
        cmds.move(-2, 0, 0, r=True)
        cmds.group('pSphere1', 'pCube1')
        cmds.move(0, 0, 2, r=True)

        cmds.refresh()

        scene = [
            # Maya objects
            '|pTorus1', 
            '|pTorus1|pTorusShape1', 
            '|group1',
            '|group1|pSphere1',
            '|group1|pSphere1|pSphereShape1',
            '|group1|pCube1',
            '|group1|pCube1|pCubeShape1']

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

    def test_isolateSelectMayaSelectionHighlighting(self):

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

        # Now select Maya objects and check both object and selection
        # highlighting visibility.
        cmds.select('|pTorus1')
        cmds.editor(modelPanel, edit=True, updateMainConnection=True)
        cmds.isolateSelect(modelPanel, loadSelected=True)

        cmds.refresh()

        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/pTorus1',
                              f='TestPrimPath.isVisible');
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/Lighted/pTorus1',
                              f='TestPrimPath.isVisible');
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/Lighted/group1',
                              f='TestPrimPath.notVisible');

        cmds.select('|group1')
        cmds.editor(modelPanel, edit=True, updateMainConnection=True)
        cmds.isolateSelect(modelPanel, loadSelected=True)

        cmds.refresh()

        # It appears that when selection is made once on a Maya object, its
        # selection highlighting (its DormantPolyWire render item) is kept, but
        # made invisible.  We therefore cannot test for selection highlighting
        # prim existence, and must rather test for invisibility.
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/pTorus1',
                              f='TestPrimPath.notVisible');
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/Lighted/pTorus1',
                              f='TestPrimPath.notVisible');
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/group1',
                              f='TestPrimPath.isVisible');
        cmds.mayaHydraCppTest('/MayaHydraViewportRenderer/rprims/Lighted/group1',
                              f='TestPrimPath.isVisible');

        disableIsolateSelect(modelPanel)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
