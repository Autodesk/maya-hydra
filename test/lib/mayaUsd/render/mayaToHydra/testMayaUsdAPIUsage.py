#
# Copyright 2024 Autodesk, Inc. All rights reserved.
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
import unittest
import mayaUtils
import platform

class TestMayaUsdAPI(mtohUtils.MayaHydraBaseTestCase): #Subclassing mtohUtils.MayaHydraBaseTestCase to be able to call self.assertSnapshotClose
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    @property
    def imageDiffFailThreshold(self):
        return 0.01
    
    @property
    def imageDiffFailPercent(self):
        # HYDRA-837 : Wireframes seem to have a slightly different color on macOS. We'll increase the thresholds
        # for that platform specifically for now, so we can still catch issues on other platforms.
        if platform.system() == "Darwin":
            return 3
        return 0.2

    def test_MovingUsdStage(self):
        # Load a maya scene with a sphere prim in a UsdStage and a directional light, with HdStorm already being the viewport renderer.
        testFile = mayaUtils.openTestScene(
                "testMayaUsdAPIUsage",
                "UsdStageWithSphereMatXStdSurf.ma")
        cmds.refresh()
        self.assertSnapshotClose("mayaUsdAPI_DirectionalLight.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Move the transform node, the usd stage prim should move as well
        # Get the transform node of the MayaUsdProxyShape node hosting the stage, it is named "stage1"
        transformNode = "stage1"
        #Select the transform node
        cmds.select(transformNode)
        # Move the selected node
        cmds.move(0, 0, -2)
        cmds.refresh()
        self.assertSnapshotClose("mayaUsdAPI_TransformMoved.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
        
        #Hide the transform node, this should hide the usd stage
        cmds.hide(transformNode)
        self.assertSnapshotClose("mayaUsdAPI__NodeHidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Unhide the transform node, this should unhide the usd stage
        cmds.showHidden(transformNode)
        self.assertSnapshotClose("mayaUsdAPI__NodeUnhidden.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Delete the shape node, this should hide the usd stage
        cmds.delete(transformNode)
        self.assertSnapshotClose("mayaUsdAPI__NodeDeleted.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)
            
        #Undo the delete, the usd stage should be visible again
        cmds.undo()
        self.assertSnapshotClose("mayaUsdAPI__NodeDeletedUndo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Redo the delete, the usd stage should be hidden
        cmds.redo()
        self.assertSnapshotClose("mayaUsdAPI__NodeDeletedRedo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Undo the delete again, the stage prims should be visible
        cmds.undo()
        self.assertSnapshotClose("mayaUsdAPI__NodeDeletedUndoAgain.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Move transform node again to see if it still updates the usd stage prims
        cmds.select(transformNode)
        # Move the selected node
        cmds.move(0, 0, 4)
        cmds.refresh()
        self.assertSnapshotClose("mayaUsdAPI__NodeMovedAfterDeletionAndUndo.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Switch to VP2
        self.setViewport2Renderer()
        #Switch back to Storm
        self.setHdStormRenderer()
        self.assertSnapshotClose("mayaUsdAPI__VP2AndThenBackToStorm.png", self.imageDiffFailThreshold, self.imageDiffFailPercent)

        #Finish by a File New command
        cmds.file(new=True, force=True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
