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
import os

class TestMeshes(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def matchingRprims(self, rprims, matching):
        return len([rprim for rprim in rprims if matching in rprim])

    # Override the base class method to provide a suffix to setUpClass().
    # 
    # This test is run twice, with different values for the
    # MAYA_HYDRA_USE_MESH_ADAPTER environment variable.  This requires creating
    # two different output directories, otherwise the two test runs will clash
    # when run in parallel, typically when trying to clear out the output
    # directory before the test runs:
    #   File "[...]\maya\builds\master\maya\build\RelWithDebInfo\runTime\Python\Lib\shutil.py", line 624, in _rmtree_unsafe
    # os.rmdir(path)
    # PermissionError: [WinError 32] The process cannot access the file because it is being used by another process: '[...]\\maya-hydra-4\\build\\Coverage\\test\\lib\\mayaUsd\\render\\mayaToHydra\\testMeshesOutput'
    @classmethod
    def setUpClass(cls):
        if cls._file is None:
            raise ValueError("Subclasses of MayaHydraBaseTestCase must "
                             "define `_file = __file__`")

        meshAdapter = os.getenv('MAYA_HYDRA_USE_MESH_ADAPTER', 0)
        fixturesUtils.setUpClass(cls._file, 'mayaHydra',
                                 initializeStandalone=False,
                                 suffix='_meshAdapter' if meshAdapter else '')

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
