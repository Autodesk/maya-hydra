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
import maya.mel as mel
import fixturesUtils
import mayaUtils
import mtohUtils

def enableIsolateSelect(modelPanel):
    # See comments in cpp/testIsolateSelect.py
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 1" % modelPanel)
    
def disableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 0" % modelPanel)

class TestIsolateSelectWithUsdLighting(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.01
    IMAGE_DIFF_FAIL_PERCENT = 0.2
    imageVersion=None
    
    def setUp(self):
        super(TestIsolateSelectWithUsdLighting, self).setUp()
        # Compute imageVersion once during setup
        frame_passes_count = self.framePassesCount
        if frame_passes_count == 2:
            self.imageVersion = "two_passes"

    def test_IsolateSelectWithUsdLighting(self):
        mayaUtils.openTestScene( 
                "testIsolateSelectWithUsdLighting",
                "mayaPlusUSDMeshesWithUSDLighting.ma")

        self.setHdStormRenderer()

        # Bring the camera in closer.
        self.setBasicCam(5)

        cmds.refresh()

        # Isolate select the USD sphere.
        modelPanel = 'modelPanel4'
        enableIsolateSelect(modelPanel)

        cmds.select('|stage1|stageShape1,/pSphere1')
        cmds.editor(modelPanel, edit=True, updateMainConnection=True)
        cmds.isolateSelect(modelPanel, loadSelected=True)

        cmds.refresh()

        imageVersion = None
        if self._usdVersion >= (0, 25, 8):
            imageVersion = "usd2508+"

        self.assertSnapshotClose("isolateSelectWithUsdLighting" + ".png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT, imageVersion)

        # Disable the isolate selection.
        disableIsolateSelect(modelPanel)

        cmds.refresh()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
