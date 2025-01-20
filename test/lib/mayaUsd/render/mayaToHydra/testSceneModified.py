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
import fixturesUtils
import mtohUtils

class TestSceneModified(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    # Base class setUp() defines HdStorm as the renderer.

    def test_sceneModified(self):
        # Though it modifies node 'defaultRenderGlobals' (as reported by
        # 'ls -modified'), loading the mayaHydra plugin should not cause
        # a scene modified warning, which causes spurious dialogs on file
        # new or open to save what is essentially an empty file.
        self.assertFalse(cmds.file(query=True, modified=True))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
