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
import usdUtils
from pxr import UsdGeom
import testUtils

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

class TestUsdPointInstancingIsolateSelectBBox(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    _pluginsToLoad = ['mayaHydraCppTests', 'mayaHydraFlowViewportAPILocator']
    _pluginsToUnload = []

    @classmethod
    def setUpClass(cls):
        super(TestUsdPointInstancingIsolateSelectBBox, cls).setUpClass()
        for p in cls._pluginsToLoad:
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

    @classmethod
    def tearDownClass(cls):
        super(TestUsdPointInstancingIsolateSelectBBox, cls).tearDownClass()
        # Clean out the scene to allow all plugins to unload cleanly.
        cmds.file(new=True, force=True)
        for p in reversed(cls._pluginsToUnload):
            if p != 'mayaHydraFlowViewportAPILocator':
                cmds.unloadPlugin(p)

    def setupScene(self):
        self.proxyShapePathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(self.proxyShapePathStr).GetStage()

        # Add in a point instancer and its point instances.
        ref = stage.DefinePrim('/ref')
        piScenePath = testUtils.getTestScene('testUsdPointInstances', 'pointInstancer.usda')
        ref.GetReferences().AddReference(str(piScenePath))
        self.pointInstancerPath = self.proxyShapePathStr + ',/ref/CubePointInstancer'

        scene = [self.pointInstancerPath]

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

    def notVisible(self, scene, visible):
        notVisible = scene.copy()

        for p in visible:
            notVisible.remove(p)

        return notVisible

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

        cmds.modelEditor(modelPanel, edit=True, displayAppearance='boundingBox')

        cmds.refresh()

        self.assertVisibility(visible, self.notVisible(scene, visible))

        disableIsolateSelect(modelPanel)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
