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
import maya.mel as mel

import fixturesUtils
import mtohUtils
import mayaUtils
from testUtils import PluginLoaded

import unittest

class TestMeshes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def matchingRprims(self, rprims, matching):
        return len([rprim for rprim in rprims if matching in rprim])

    @unittest.skipUnless(mayaUtils.hydraFixLevel() > 0, "Requires Data Server render item lifescope fix.")
    def test_sweepMesh(self):
        self.setHdStormRenderer()
        with PluginLoaded('sweep'):
            mel.eval("performSweepMesh 0")
            cmds.refresh()

            # There should be a single rprim from the sweep shape.
            rprims = self.getIndex()
            self.assertEqual(1, self.matchingRprims(rprims, 'sweepShape'))

            # Change the scale profile.
            cmds.setAttr("sweepMeshCreator1.scaleProfileX", 2)
            cmds.refresh()

            # Should still be a single rprim from the sweep shape.
            rprims = self.getIndex()
            self.assertEqual(1, self.matchingRprims(rprims, 'sweepShape'))

    def test_meshSolid(self):
        '''Test that meshes are under the common Solid root for lighting / shadowing.'''
        self.setHdStormRenderer()
        cmds.polySphere(r=1, sx=20, sy=20, ax=[0, 1, 0], cuv=2 , ch=1)
        cmds.refresh()

        # There should be two rprims from the poly sphere, one for the mesh and
        # another wireframe for selection highlighting.
        rprims = self.getIndex()
        self.assertEqual(2, len(rprims))

        # Only the mesh rprim is a child of the "Lighted" hierarchy.
        self.assertEqual(1, self.matchingRprims(rprims, 'Lighted'))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
