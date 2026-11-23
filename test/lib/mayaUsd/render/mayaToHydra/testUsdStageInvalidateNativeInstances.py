# Copyright 2026 Autodesk
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
import testUtils
import usdUtils

PROXY_SHAPE_PREFIX = '/MayaUsdProxyShape_PluginNode/'

class TestUsdStageInvalidateNativeInstances(mtohUtils.MayaHydraBaseTestCase):
    '''Invalidating a stage already in Hydra must not leave stale prims (HYDRA-2582).

    Stage From File, and any edit of filePath or primPath, invalidate the stage after its first
    population. Without the fix, test_stageInvalidate gets 5 Storm rprims for
    nestedNativeInstances.usda (duplicated ids and a raw __Prototype_N prim) instead of 1.
    '''
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    STAGE_SHAPE = 'nestedNativeInstancesShape'

    def setUp(self):
        super(TestUsdStageInvalidateNativeInstances, self).setUp()
        # Rprims are counted in Storm's render index.
        self.setHdStormRenderer()

    def loadStage(self):
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', 'nestedNativeInstances.usda')
        usdUtils.createStageFromFile(usdScenePath)
        cmds.select(clear=True)
        cmds.refresh(force=True)

    def usdRprims(self, visibleOnly=False):
        cmds.refresh(force=True)
        rprims = cmds.mayaHydra(renderer='HdStormRendererPlugin', listRenderIndex=True,
                                visibleOnly=visibleOnly) or []
        return [rprim for rprim in rprims if rprim.startswith(PROXY_SHAPE_PREFIX)]

    def assertSingleCube(self, when):
        # All the cubes of the file are instances of a single prototype prim.
        rprims = self.usdRprims()
        self.assertEqual(len(rprims), 1, '{}: {}'.format(when, rprims))

    def test_loadIntoRunningHydra(self):
        self.loadStage()
        self.assertSingleCube('after loading the stage')

    def test_stageInvalidate(self):
        self.loadStage()
        self.assertSingleCube('after loading the stage')

        # Same value: MayaUSD still invalidates the stage.
        cmds.setAttr(self.STAGE_SHAPE + '.primPath', '', type='string')
        self.assertSingleCube('after the stage invalidate')

        # The scene index chain that replaced the first one must handle an invalidate too.
        cmds.setAttr(self.STAGE_SHAPE + '.primPath', '', type='string')
        self.assertSingleCube('after a second stage invalidate')

    def test_stageInvalidateOfHiddenProxyShape(self):
        # One refresh after the invalidate must not show the hidden stage. No native instancing
        # here: their prototype rprims stay visible, the instancer hides them.
        usdScenePath = testUtils.getTestScene('testStagePayloadsReferences', 'cube.usda')
        shape = usdUtils.createStageFromFile(usdScenePath)
        cmds.select(clear=True)
        self.assertNotEqual(self.usdRprims(visibleOnly=True), [], 'after loading the stage')
        transform = cmds.listRelatives(shape, parent=True, fullPath=True)[0]
        cmds.setAttr(transform + '.visibility', False)
        self.assertEqual(self.usdRprims(visibleOnly=True), [], 'after hiding the proxy shape')

        cmds.setAttr(shape + '.primPath', '', type='string')
        self.assertEqual(self.usdRprims(visibleOnly=True), [], 'after the stage invalidate')
        # The stage is still in Hydra, only hidden.
        self.assertNotEqual(self.usdRprims(), [], 'after the stage invalidate')

    def test_stageInvalidateAfterRename(self):
        # The replacement keeps the Hydra paths (held by selection and isolate select), even
        # after a rename.
        self.loadStage()
        self.assertSingleCube('after loading the stage')
        rprimsBefore = self.usdRprims()

        renamedShape = cmds.rename(self.STAGE_SHAPE, 'renamedStageShape')
        cmds.setAttr(renamedShape + '.primPath', '', type='string')
        self.assertEqual(self.usdRprims(), rprimsBefore, 'after a rename and a stage invalidate')

    def test_reloadAfterFileNew(self):
        self.loadStage()
        self.assertSingleCube('after loading the stage')

        cmds.file(new=True, force=True)
        self.assertEqual(self.usdRprims(), [], 'after File > New')

        self.loadStage()
        self.assertSingleCube('after loading the stage again')

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
