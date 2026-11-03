#
# Copyright 2026 Autodesk, Inc. All rights reserved.
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
import mayaUtils
import mtohUtils

class TestUsdAreaLightGizmos(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    IMAGE_DIFF_FAIL_PERCENT = 1

    def test_UsdSphereLightGizmo(self):
        import mayaUsd
        import mayaUsd_createStageWithNewLayer
        from pxr import UsdGeom, UsdLux

        mayaUtils.openNewScene()
        self.setTestRenderer()
        self.setBasicCam(dist=1)

        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        stage = mayaUsd.lib.GetPrim(stagePath).GetStage()
        UsdGeom.Xform.Define(stage, "/AreaLightXform")
        UsdLux.SphereLight.Define(stage, "/AreaLightXform/SphereLight1")

        cmds.modelEditor(mayaUtils.activeModelPanel(), edit=True, displayLights="all")
        cmds.select(clear=True)
        cmds.refresh()

        self.assertSnapshotClose(
            "usdSphereLightGizmo.png",
            self.IMAGE_DIFF_FAIL_THRESHOLD,
            self.IMAGE_DIFF_FAIL_PERCENT)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
