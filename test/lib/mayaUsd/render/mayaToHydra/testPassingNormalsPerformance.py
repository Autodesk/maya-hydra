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
import mtohUtils
import mayaUtils
import unittest
import os
import sys

class TestPassingNormalsPerformanceWo(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _perfTestFiles = ["dinosaur_maya_geo.mb", "BigFiles.mb", "TumbleMonster.mb", "warMachinAnimated.mb", 
                    "Synthetic_Dense_Mesh_80M_Triangles.mb", "lots_of_spheres_random_colors_baked_test.mb"]

    @staticmethod
    def setupScene(testfilename):
        cmds.file(new=True, force=True)
        testFile = mayaUtils.openTestScene(
                "testPassingNormals",
                testfilename)
        cmds.refresh(f=True)

    def mesurePerformance(self):
        #Set VP2 as the renderer
        self.setViewport2Renderer()
        cmds.refresh(f=True)
        start = cmds.timerX()
        #Set Storm as the renderer
        self.setHdStormRenderer()
        cmds.refresh(f=True)
        time1 = cmds.timerX(startTime=start)
        return time1
    
    #
    # Tumble the specified camera in specified frames, and repeat specified times.
    #
    #   camera         The camera to tumble, 'persp' by default
    #   frames         The number of frames to tumble, 100 by default
    #   iterations     The number of times to repeat, 3 by default
    #
    #   RETURN         Frame Per Second
    #
    @staticmethod
    def tumble_perf(camera='persp', frames=100, iterations=3):
        # switch to the camera
        cmds.lookThru(camera)
        # flush any pending update
        cmds.refresh(f=True)
        start = cmds.timerX()
        for i in range(iterations):
            for j in range(frames):
                cmds.tumble(aa=float(360)/frames)
                cmds.refresh(f=True)
        elapsed_time = cmds.timerX(startTime=start)
        fps = frames*iterations / elapsed_time
        return fps

    @staticmethod
    def anim_perf(camera='persp', frames=100, iterations=5):
        # switch to the camera
        cmds.lookThru(camera)
        # flush any pending update
        cmds.refresh(f=True)
        start = cmds.timerX()
        for i in range(iterations):
            cmds.currentTime(0, edit=True) #Reset time to 0 for anim
            for j in range(frames):
                current_time = cmds.currentTime(query=True)
                cmds.currentTime(current_time + 1, edit=True)
                cmds.refresh()
        elapsed_time = cmds.timerX(startTime=start)
        fps = frames*iterations / elapsed_time
        return fps

    def test_Normals(self):
        
        numVp2StormIterations = 5
        
        for testfilename in self._perfTestFiles:
            self.setupScene(testfilename)
            totalTime = 0
            for i in range(numVp2StormIterations):
                totalTime += self.mesurePerformance()
            totalTime /= numVp2StormIterations
            
            self.setHdStormRenderer()
            cmds.refresh(f=True)
            fps = self.tumble_perf()
        
            sys.__stdout__.write("\nScene : {} Time : {} FPS : {}\n\n".format(testfilename, totalTime, fps))
            sys.__stdout__.flush()

    def test_NormalsWithAnimation(self):

        testFile = mayaUtils.openTestScene(
                "testPassingNormals",
                "Synthetic_Mesh_8M_Triangles_Deformed.mb")
        self.setHdStormRenderer()
        cmds.refresh(f=True)
        
        StormFps = self.anim_perf()
        self.setViewport2Renderer()
        cmds.refresh(f=True)
        VP2Fps = self.anim_perf()
        
        sys.__stdout__.write("Anim performance FPS : Storm {}, VP2 : {}]\n\n".format(StormFps, VP2Fps))
        sys.__stdout__.flush()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
