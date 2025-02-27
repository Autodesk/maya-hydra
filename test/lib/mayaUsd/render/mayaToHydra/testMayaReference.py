# Copyright 2025 Autodesk
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

class TestMayaReference(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _requiredPlugins = ['mayaHydraCppTests']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 2

    def referenceScene(self):

        # Add a non-referenced object in the scene so that the scene
        # with the unloaded reference will still contain an object.
        cmds.polySphere()
        cmds.move(0, 0, -2)

        # Move the camera in closer
        self.setBasicCam(5)

        testFile = testUtils.getTestScene("testMayaReference", "aCube.ma")
        return cmds.file(testFile, reference=True, namespace='aCube')

    def test_LoadMayaReference(self):

        file = self.referenceScene()
        refNode = cmds.file(file, query=True, referenceNode=True)

        cmds.refresh()

        self.assertSnapshotClose('loadMayaReference.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

        # Unload the reference
        cmds.file(unloadReference=refNode)

        cmds.refresh()

        self.assertSnapshotClose('unloadMayaReference.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_EditMayaReference(self):

        self.referenceScene()

        cmds.move(0, 0, 2, '|aCube:pCube1')

        cmds.refresh()

        self.assertSnapshotClose('editMayaReference.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

    def test_RemoveMayaReference(self):

        file = self.referenceScene()

        cmds.refresh()

        # Remove the reference
        cmds.file(file, removeReference=True)

        cmds.refresh()

        # Same snapshot as unloaded reference.
        self.assertSnapshotClose('unloadMayaReference.png', self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
