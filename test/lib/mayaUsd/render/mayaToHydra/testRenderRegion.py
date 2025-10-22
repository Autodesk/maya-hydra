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
import platform

class TestRenderRegion(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    IMAGEDIFF_FAIL_THRESHOLD = 0.01

    IMAGEDIFF_FAIL_PERCENT = 0.1

    def setupScene(self):
        import mayaUsd_createStageWithNewLayer
        self.setHdStormRenderer()
        cmds.refresh()
        self.setBasicCam(15)
        torusName = cmds.polyTorus()[0]
        cmds.xform(translation = [0, 5, 0], scale=[3, 3, 3])
        stagePath = mayaUsd_createStageWithNewLayer.createStageWithNewLayer()
        cubeName = cmds.polySphere()[0]
        cmds.xform(translation = [0, -5, 0], scale=[5, 5, 5])
        cmds.mayaUsdDuplicate(cmds.ls(cubeName, long=True)[0], stagePath)
        cmds.delete(cubeName)
        cmds.refresh()
        cmds.select(clear=True)

    def verifySnapshot(self, imageName):
        cmds.refresh()
        self.assertSnapshotClose(imageName,
                                 self.IMAGEDIFF_FAIL_THRESHOLD,
                                 self.IMAGEDIFF_FAIL_PERCENT)

    def test_RenderRegion(self):
        self.setupScene()

        # Test a render region that occupies only part of the viewport.
        # Since the unit tests run a playblast with a 400x400 viewport,
        # we cannot just get the current viewport coordinates and divide
        # them in half, we need to hardcode them.
        visibleRegion = [0, 0, 225, 275]
        cmds.mayaHydraRenderRegion(edit=True, region=visibleRegion)
        self.verifySnapshot("mayaHydraRenderRegion_partially_visible.png")
        actualVisibleRegion = cmds.mayaHydraRenderRegion(query=True, region=True)
        self.assertEqual(actualVisibleRegion, visibleRegion)

        # Test with a render region that is out of view, so objects will not
        # be rendered.
        outOfViewRegion = [0, 0, 3, 5]
        cmds.mayaHydraRenderRegion(edit=True, region=outOfViewRegion)
        self.verifySnapshot("mayaHydraRenderRegion_out_of_view.png")
        actualOutOfViewRegion = cmds.mayaHydraRenderRegion(query=True, region=True)
        self.assertEqual(actualOutOfViewRegion, outOfViewRegion)
        
        # Test without any render region.
        cmds.mayaHydraRenderRegion(edit=True, clear=True)
        self.verifySnapshot("mayaHydraRenderRegion_no_region.png")
        with self.assertRaises(RuntimeError):
            cmds.mayaHydraRenderRegion(query=True, region=True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
