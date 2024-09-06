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

class TestUsdPointInstancePicking(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    PICK_PATH = "|nestedPointInstancers|nestedPointInstancersShape,/Root"

    # The test picking framework expects rprims that have a (flattened)
    # transform, to determine their projected mouse coordinates.  Point
    # instancers generate point instance geometry from prototypes, so we cannot
    # directly obtain a flattened transform for them.  To work around this, we
    # add marker objects to the scene that are co-located with point instances
    # we want to pick, and use the marker objects to determine the projected
    # mouse coordinates.
    def loadUsdScene(self):
        usdScenePath = testUtils.getTestScene('testUsdPointInstances', 'nestedPointInstancers.usda')
        usdUtils.createStageFromFile(usdScenePath)

    def setUp(self):
        super(TestUsdPointInstancePicking, self).setUp()
        self.loadUsdScene()
        cmds.setAttr('persp.translate', 15, 15, 10, type='float3')
        cmds.setAttr('persp.rotate', -40, 60, 0, type='float3')
        cmds.refresh()

    def test_PickPointInstancer(self):
        with PluginLoaded('mayaHydraCppTests'):

            cmds.optionVar(
                sv=('mayaUsd_PointInstancesPickMode', 'PointInstancer'))

            # In point instancer mode picking an instance will select the 
            # top-level point instancer, regardless of the picked instance.
            # We therefore pass in two markers to pick, and the result should
            # be the same top-level point instancer selection.
            markers = ["/MarkerRedPyramidInstance3", "/MarkerBlueCubeInstance0"]
            for marker in markers:
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + "/ParentPointInstancer",
                    self.PICK_PATH + marker,
                    f="TestUsdPicking.pickPrimWithMarker")

    def test_PickInstances(self):
        with PluginLoaded('mayaHydraCppTests'):

            cmds.optionVar(
                sv=('mayaUsd_PointInstancesPickMode', 'Instances'))

            # In instances mode picking an instance will select the 
            # top-level instance.
            markers = ["/MarkerRedPyramidInstance3",
                       "/MarkerGreenPyramidInstance1", 
                       "/MarkerBlueCubeInstance0",
                       "/MarkerYellowCubeInstance2"]
            instances = ["/ParentPointInstancer/3",
                         "/ParentPointInstancer/1",
                         "/ParentPointInstancer/0",
                         "/ParentPointInstancer/2"]

            for (marker, instance) in zip(markers, instances):
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + instance,
                    self.PICK_PATH + marker,
                    f="TestUsdPicking.pickInstanceWithMarker")

    def test_PickPrototypes(self):
        with PluginLoaded('mayaHydraCppTests'):

            cmds.optionVar(
                sv=('mayaUsd_PointInstancesPickMode', 'Prototypes'))

            # In prototypes mode picking an instance will select the prototype
            # of the most nested instance.
            markers = ["/MarkerRedPyramidInstance3",
                       "/MarkerGreenPyramidInstance1", 
                       "/MarkerBlueCubeInstance0",
                       "/MarkerYellowCubeInstance2"]
            prototypes = [
                "/ParentPointInstancer/prototypes/PyramidPointInstancerXform/PyramidPointInstancer/prototypes/RedPyramid/Geom/Pyramid",
                "/ParentPointInstancer/prototypes/PyramidPointInstancerXform/PyramidPointInstancer/prototypes/GreenPyramid/Geom/Pyramid",
                "/ParentPointInstancer/prototypes/CubePointInstancerXform/CubePointInstancer/prototypes/BlueCube/Geom/Cube",
                "/ParentPointInstancer/prototypes/CubePointInstancerXform/CubePointInstancer/prototypes/YellowCube/Geom/Cube"]

            for (marker, prototype) in zip(markers, prototypes):
                cmds.mayaHydraCppTest(
                    self.PICK_PATH + prototype,
                    self.PICK_PATH + marker,
                    f="TestUsdPicking.pickPrimWithMarker")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
