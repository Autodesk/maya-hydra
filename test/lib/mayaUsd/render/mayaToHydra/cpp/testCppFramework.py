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
from testUtils import PluginLoaded

# Test to ensure the C++ Testing Framework works as expected 

class TestCppFramework(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_Pass(self):
        self.setHdStormRenderer()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="CppFramework.pass")


    def test_Fail(self):
        self.setHdStormRenderer()
        with PluginLoaded('mayaHydraCppTests'):
            with self.assertRaises(RuntimeError):
                cmds.mayaHydraCppTest(f="CppFramework.fail")

if __name__ == '__main__':
    fixturesUtils.runTests(globals())