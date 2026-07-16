# Copyright 2026 Autodesk
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
# Python wrapper for testFvpDirtyNotifier.cpp. Loads mayaHydraCppTests and runs the
# DirtyNotifier GTest suite via mayaHydraCppTest (no scene setup required).
#
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded


class TestDirtyNotifier(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    # What: DirtyNotifier emissions must match HdDirtyBitsTranslator output for every
    #       supported HdDirtyBits combination (the migration gate).
    # How:  Run the full DirtyNotifier GTest suite via mayaHydraCppTest.
    # Expect: all test cases in the suite pass.
    def test_locatorEquivalence(self):
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="DirtyNotifier.*")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
