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
import testUtils
import usdUtils

class TestSelectPrimWithoutDataSource(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_selectPrimWithoutDataSource(self):

        # Importing mayaUsd or mayaUsd.lib at module scope produces an "unbound
        # local" error on use, so import in local scope.  May be tied to 
        # point at which mayaUsd plugin gets loaded into Maya.
        import mayaUsd.lib

        # Prims without a data source are sometimes used to flag illegal prim
        # return values, but such prims can also be legitimately used as parent
        # prims for children prims that do have data sources.  As such a prim
        # without a data source must be selectable.

        # Read in a scene that creates a legal Hydra parent prim without a data
        # source.
        usdScenePath = testUtils.getTestScene('testSelectPrimWithoutDataSource', 'root.usda')

        self.proxyShapePathStr = usdUtils.createStageFromFile(usdScenePath)

        stage = mayaUsd.lib.GetPrim(self.proxyShapePathStr).GetStage()

        self.assertIsNotNone(stage)

        # Initially select the proxy shape.
        cmds.select('|root|rootShape')
        cmds.refresh()

        # Switch selection to a USD prim that generates a Hydra prim without
        # a data source, but with children.  This must not crash.
        cmds.select('|root|rootShape,/root/refAssetB')
        cmds.refresh()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
