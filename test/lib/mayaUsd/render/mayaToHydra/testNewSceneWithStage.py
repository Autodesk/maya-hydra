# Copyright 2023 Autodesk
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

class TestStage(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_newFileWithUsdStage(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib

        self.setHdStormRenderer()
        # create an empty USD Stage
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        
        # create a new Maya scene.
        # See https://jira.autodesk.com/browse/HYDRA-496
        # prior to this fix, Maya would crash
        cmds.file( f=True, new=True )     
        
        # sanity check
        self.setHdStormRenderer()
        # start with a maya sphere
        sphere = cmds.polySphere()
        cmds.refresh()
        rprims = self.getIndex()
        # we expect non-zero rprims
        rprimsCount = len(rprims)
        self.assertGreater(rprimsCount, 0)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
