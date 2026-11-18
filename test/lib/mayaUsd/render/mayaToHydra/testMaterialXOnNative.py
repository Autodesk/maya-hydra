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
import maya.utils
import mayaUtils
import fixturesUtils
import mtohUtils
from maya.api import OpenMaya

class TestMaterialXOnNative(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['LookdevXMaya']

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT)

    def test_MaterialX(self):
        mayaUtils.openTestScene("testMaterialX", "RedMtlxSphere.ma")
        self.setBasicCam(2)
        self.setTestRenderer()
        self.verifySnapshot("RedMtlxSphere.png")

    def test_NoUnresolvedNodeDefWarning(self):
        # The material references standard library node definitions without including
        # them; they must not be reported as unresolved.
        messages = []
        def commandOutputCallback(message, _messageType, _clientData):
            messages.append(message)

        callbackId = OpenMaya.MCommandMessage.addCommandOutputCallback(commandOutputCallback)
        try:
            mayaUtils.openTestScene("testMaterialX", "RedMtlxSphere.ma")
            self.setBasicCam(2)
            self.setTestRenderer()
            # The red sphere shows that the material was built from MaterialX,
            # where the node definitions are checked.
            self.verifySnapshot("RedMtlxSphere.png")
            # Warnings from other threads can be delivered on idle.
            maya.utils.processIdleEvents()
        finally:
            OpenMaya.MMessage.removeCallback(callbackId)

        unresolved = [m for m in messages if "could not be resolved" in m]
        self.assertEqual(unresolved, [])

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
