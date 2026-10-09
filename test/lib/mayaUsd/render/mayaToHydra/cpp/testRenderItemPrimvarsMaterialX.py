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
# Python wrapper for the MaterialX cases of testRenderItemPrimvars.cpp in render-items mode
# (no mesh adapter env var). Kept apart from testRenderItemPrimvars.py because it needs LookdevX.
#
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils
from testUtils import PluginLoaded


class TestRenderItemPrimvarsMaterialX(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['LookdevXMaya']

    # What: untextured MaterialX shader must not advertise unpopulated primvars (st, tangents).
    # How: open RedMtlxSphere.ma, then run the C++ test on the sphere render items.
    # Expect: declared normals/st/tangents cover every point the topology references.
    def test_untexturedMaterialXDeclaresOnlyPopulatedPrimvars(self):
        mayaUtils.openTestScene("testMaterialX", "RedMtlxSphere.ma")
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhMeshShape", cmds.ls("pSphereShape1", long=True)[0]))
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                f="RenderItemPrimvars.UntexturedMaterialXDeclaresOnlyPopulatedPrimvars")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
