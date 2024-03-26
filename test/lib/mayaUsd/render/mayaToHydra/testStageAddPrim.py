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
          
    def test_addPrim(self):
        import mayaUsd_createStageWithNewLayer
        import mayaUsd.lib

        self.setHdStormRenderer()
        # empty scene and expect zero prims
        rprims = self.getIndex()
        self.assertEqual(0, len(rprims))
        
        # start with a maya sphere
        sphere = cmds.polySphere()
        cmds.refresh()
        rprims = self.getIndex()
        # we expect non-zero rprims(two here)
        rprimsBefore = len(rprims)
        self.assertGreater(rprimsBefore, 0)
        
        # add an empty USD Stage.
        psPathStr = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        
        # duplicate the sphere into Stage as USD data
        self.assertTrue(mayaUsd.lib.PrimUpdaterManager.duplicate(cmds.ls(sphere[0], long=True)[0], psPathStr))
        cmds.refresh()
        rprims = self.getIndex()
        rprimsAfter = len(rprims)
        self.assertGreater(rprimsAfter, rprimsBefore)
        cmds.delete(sphere[0])
        
        cmds.refresh()
        rprims = self.getIndex()
        # we expect a non-zero rprim count(one here)
        rprimsAfter = len(rprims)
        self.assertGreater(rprimsAfter, 0)
            

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
