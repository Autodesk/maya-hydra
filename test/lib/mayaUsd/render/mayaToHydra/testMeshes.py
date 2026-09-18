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
import testUtils
from pxr import Usd

import unittest
import os

MAYAUSD_PLUGIN_NAME = 'mayaUsdPlugin'

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

        # Set up the custom suffix for this test. The selection highlighting
        # mode contributes to it as well, since this script is registered once
        # per mode too and each run needs its own output directory.
        meshAdapter = os.getenv('MAYA_HYDRA_USE_MESH_ADAPTER', 0)
        customSuffix = '_meshAdapter' if meshAdapter else ''
        customSuffix += cls.selectionHighlightOutputSuffix()
        
        # Call fixturesUtils.setUpClass with our custom suffix
        inputPath = fixturesUtils.setUpClass(
            cls._file, 'mayaHydra', initializeStandalone=False, 
            suffix=customSuffix)

        # Set up input directory like the parent class does
        if cls._inputDir is None:
            inputDirName = os.path.splitext(os.path.basename(cls._file))[0]
            inputDirName = testUtils.stripPrefix(inputDirName, 'test')
            if not inputDirName.endswith('Test'):
                inputDirName += 'Test'
            cls._inputDir = os.path.join(inputPath, inputDirName)

        cls._testDir = os.path.abspath('.')

        # This optionVar sets the color management status used when creating a new scene.
        # We set it to off to have color management turned off by default before each test
        # (as setUp creates a new file), and to avoid inadvertently turning it on mid-test
        # if a new file is manually created.
        cmds.optionVar(intValue=('colorManagementEnabledByDefault', 0))

        if MAYAUSD_PLUGIN_NAME not in cls._requiredPlugins:
            cls._requiredPlugins.append(MAYAUSD_PLUGIN_NAME)

        for p in cls._requiredPlugins:
            # If a plugin fails to load, the entire test suite will be immediately aborted.
            # Note that in the case of mtoa, the plugin might load successfully but not
            # initialize properly, which means issues will only be caught in the actual tests.
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

        #Set the usd version
        cls._usdVersion = Usd.GetVersion()

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

        # There should be one rprim for the poly sphere mesh, plus one more in
        # legacy mode, where the selection highlight of the newly created (and so
        # selected) sphere is drawn as wireframe geometry.
        rprims = self.getIndex()
        self.assertEqual(1 + self.selectionHighlightRprimCount(), len(rprims))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
