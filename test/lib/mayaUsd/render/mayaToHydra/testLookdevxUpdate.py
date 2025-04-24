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
import mayaUtils
import fixturesUtils
import mtohUtils
import maya.mel as mel

class TestLookdevxUpdate(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['LookdevXMaya']

    IMAGEDIFF_FAIL_THRESHOLD = 0.01
    IMAGEDIFF_FAIL_PERCENT = 0.1

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(imageName, 
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT)

    def test_LookdevxUpdate(self):
        mayaUtils.openTestScene("testLookdevx", "lookdevx_node_update_hydra.ma")
        self.setHdStormRenderer()
        self.verifySnapshot("lookdevXBeforeUpdate.png")
        #Update the number of instances in the bifrost graph
        mel.eval("vnnNode \"|createPointInstancerFromUsdFile|createPointInstancerFromUsdFileShape\" \"/scatter_points\" -setPortDefaultValues \"amount\" \"150\";")
        self.verifySnapshot("lookdevXAfterUpdate.png")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
