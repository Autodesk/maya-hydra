# Copyright 2023 Autodesk
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

class TestRendererSwitching(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # We found a crash that happened when switching from VP2 -> Hydra -> VP2 -> Hydra.
    # The crash was related to Hydra resources management, where some resources were not 
    # properly unloaded. The issue would then only become apparent when trying to recreate 
    # the resources by switching to Hydra a second time : this is what this test is for.
    def test_RendererSwitching(self):
        cmds.polySphere() # The crash only occurred when the scene was not empty, so create a dummy sphere.
        self.setHdStormRenderer()
        self.setViewport2Renderer()
        self.setHdStormRenderer()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
