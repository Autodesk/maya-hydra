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
import os
import shutil
import tempfile

import maya.cmds as cmds
import maya.utils

import fixturesUtils
import mayaUtils
import mtohUtils

class TestSceneModified(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    # Start from a pristine new scene and keep the base class from resetting the
    # modified flag, so these tests observe what Hydra initialization does to it.
    # Hydra is activated explicitly by each test.
    _setTestRenderer = False
    _clearModifiedAfterSetUp = False

    def _assertSceneUnmodified(self, when):
        self.assertFalse(
            cmds.file(query=True, modified=True),
            'Scene is marked as modified {}.'.format(when))
        # 'ls -modified' lists the modified nodes, which makes a failure
        # self-explanatory: it names the nodes Maya-Hydra touched.
        self.assertEqual(
            cmds.ls(modified=True) or [], [],
            'Nodes are marked as modified {}.'.format(when))

    def _assertSceneModified(self, when):
        self.assertTrue(
            cmds.file(query=True, modified=True),
            'Scene is no longer marked as modified {}.'.format(when))

    @staticmethod
    def _refreshAndDrainIdle():
        # The first refresh initializes Hydra, which queues work on the idle
        # queue.  Drain it and refresh again, without touching the scene.
        cmds.refresh(force=True)
        maya.utils.processIdleEvents()
        cmds.refresh(force=True)

    def _activateHydra(self):
        self.setTestRenderer()
        self._refreshAndDrainIdle()

    def _reinitializeHydra(self):
        # Leaving Hydra tears down its resources and going back re-initializes
        # them.  This is what happens when Maya moves the viewport to Hydra
        # after the plugin is loaded (the "Use Hydra" preference).
        self.setViewport2Renderer()
        self._activateHydra()

    def test_sceneModified(self):
        # Though it modifies node 'defaultRenderGlobals' (as reported by
        # 'ls -modified'), loading the mayaHydra plugin should not cause
        # a scene modified warning, which causes spurious dialogs on file
        # new or open to save what is essentially an empty file.
        self.assertFalse(cmds.file(query=True, modified=True))

    def test_hydraInitializationKeepsNewSceneUnmodified(self):
        # Hydra initializing after the plugin is loaded adds its settings to
        # defaultRenderGlobals and populates UsdDefaultRenderDescription.  Neither
        # is a user edit: a new scene must stay unmodified, with no reset of the
        # modified flag by the test.
        self._assertSceneUnmodified('before Hydra is activated')

        self._activateHydra()
        self._assertSceneUnmodified('after Hydra is activated')

        self._reinitializeHydra()
        self._assertSceneUnmodified('after Hydra is re-initialized')

    def test_hydraInitializationAfterNewKeepsSceneUnmodified(self):
        self._activateHydra()

        # A new scene discards the dynamic defaultRenderGlobals attributes, so
        # the next Hydra initialization has to add them again.
        mayaUtils.openNewScene()
        self._assertSceneUnmodified('after a new scene')

        self._reinitializeHydra()
        self._assertSceneUnmodified('after Hydra is re-initialized on a new scene')

    def test_hydraInitializationAfterOpenKeepsSceneUnmodified(self):
        self._activateHydra()

        tempDir = tempfile.mkdtemp(prefix='mayaHydraSceneModified')
        self.addCleanup(shutil.rmtree, tempDir, ignore_errors=True)
        scenePath = os.path.join(tempDir, 'sceneModified.ma')

        cmds.polyCube()
        cmds.file(rename=scenePath)
        cmds.file(save=True, type='mayaAscii')
        self._assertSceneUnmodified('after saving the scene')

        mayaUtils.openNewScene()
        cmds.file(scenePath, open=True, force=True)
        self._assertSceneUnmodified('after opening a scene')

        self._reinitializeHydra()
        self._assertSceneUnmodified('after Hydra is re-initialized on an opened scene')

    def test_hydraInitializationKeepsModifiedSceneModified(self):
        # The guard must only undo what Hydra itself did: a scene the user has
        # really modified has to stay modified.
        cmds.polyCube()
        self._assertSceneModified('after the user created an object')

        self._activateHydra()
        self._assertSceneModified('after Hydra is activated')

        self._reinitializeHydra()
        self._assertSceneModified('after Hydra is re-initialized')

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
