#
# Copyright 2025 Autodesk, Inc. All rights reserved.
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

import fixturesUtils
import mtohUtils
import ufe
import testUtils
import maya.cmds as cmds

class TestDepthBias(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = ['mtoa']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def setUp(self):
        super(TestDepthBias, self).setUp()
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', 2, 2, 2, type='float3')
        self.modifyDefaultLightIntensityByUsdVersion()
        cmds.refresh()
        if self.framePassesCount < 2:
            self.skipTest("Insufficient number of render passes, need at least 2 to test depth bias")

    def test_DepthBias(self):
        import mayaUsd_createStageWithNewLayer
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        objectName = cmds.polySphere()[0]
        cmds.mayaUsdDuplicate(cmds.ls(objectName, long=True)[0], stagePath)
        cmds.delete(objectName)
        self.setHdArnoldRenderer()
        cmds.refresh()
        self.assertSnapshotClose("hd_arnold_wireframe_highlight.png", self.IMAGE_DIFF_FAIL_THRESHOLD, self.IMAGE_DIFF_FAIL_PERCENT)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
