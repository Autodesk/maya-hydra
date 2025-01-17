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
import mayaUtils
import mtohUtils

def enableIsolateSelect(modelPanel):
    # See comments in cpp/testIsolateSelect.py
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 1" % modelPanel)
    
def disableIsolateSelect(modelPanel):
    cmds.setFocus(modelPanel)
    mel.eval("enableIsolateSelect %s 0" % modelPanel)

class TestIsolateSelectWithGeomSubset(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def test_IsolateSelectWithGeomSubset(self):
        # HYDRA-1242: hang on exit of isolate select if there is a GeomSubset
        # in the geometry that is hidden.
        mayaUtils.openTestScene( 
                "testIsolateSelectWithGeomSubset",
                "mayaSpherePlusUSD_Instancer.ma")

        cmds.refresh()

        # Isolate select the Maya sphere.
        modelPanel = 'modelPanel4'
        enableIsolateSelect(modelPanel)

        cmds.select('|pSphere1')
        cmds.editor(modelPanel, edit=True, updateMainConnection=True)
        cmds.isolateSelect(modelPanel, loadSelected=True)

        cmds.refresh()

        # Disable the isolate selection.
        disableIsolateSelect(modelPanel)

        cmds.refresh()

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
