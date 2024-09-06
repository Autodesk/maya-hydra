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
import usdUtils

import testUtils
from testUtils import PluginLoaded

class TestUsdNativeInstancePicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    PICK_PATH = "|instancedCubeHierarchies|instancedCubeHierarchiesShape,/cubeHierarchies"

    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testUsdNativeInstances', 'instancedCubeHierarchies.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestUsdNativeInstancePicking, self).setUp()
        self.loadUsdScene()
        cmds.select(clear=True)
        cmds.setAttr('persp.translate', 0, 0, 15, type='float3')
        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        cmds.refresh()

    def test_NativeInstances(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.optionVar(
                    sv=('mayaUsd_PointInstancesPickMode', 'Instances'))
            
            instances = ["/cubes_1", "/cubes_2"]
            for instance in instances:
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + instance,
                    f="TestUsdPicking.pickPrim")
    
    def test_NativeInstancePrototypes(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.optionVar(
                    sv=('mayaUsd_PointInstancesPickMode', 'Prototypes'))

            # In prototypes mode, picking an instance will select the most nested
            # subprim of the prototype at the picked location.
            markers = ["/marker_cubes_1_baseCube", "/marker_cubes_2_topCube"]
            prototypes = ["/cubes_1/baseCube", "/cubes_2/topCube"]

            for (marker, prototype) in zip(markers, prototypes):
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + prototype,
                    self.PICK_PATH + marker,
                    f="TestUsdPicking.pickInstanceWithMarker")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
