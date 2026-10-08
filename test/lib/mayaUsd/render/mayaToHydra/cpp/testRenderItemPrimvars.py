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
# Python wrapper for testRenderItemPrimvars.cpp in render-items mode (no mesh adapter env var).
# The MaterialX case, which needs LookdevX, is in testRenderItemPrimvarsMaterialX.py.
#
import maya.cmds as cmds
import fixturesUtils
import mayaUtils
import mtohUtils
from testUtils import PluginLoaded


class TestRenderItemPrimvars(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def setupScene(self, sceneFolder, sceneFile, meshShape):
        mayaUtils.openTestScene(sceneFolder, sceneFile)
        self.setHdStormRenderer()
        cmds.optionVar(stringValue=("mhMeshShape", cmds.ls(meshShape, long=True)[0]))
        cmds.refresh()

    # What: a file-textured shader must keep its UVs.
    # How: open testUVs.ma, then run the C++ test on a textured plane's render items.
    # Expect: st is declared and every declared primvar is populated.
    def test_texturedMeshDeclaresPopulatedUVs(self):
        self.setupScene("testUVandUDIM", "testUVs.ma", "pPlaneShape1")
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="RenderItemPrimvars.TexturedMeshDeclaresPopulatedUVs")

    # What: a shader losing its textures must withdraw the UVs of the mesh render item.
    # How: open testUVs.ma, disconnect the file textures of blinn1 (the render item is kept),
    #      then run the C++ test on the plane.
    # Expect: st is no longer declared on the plane render items.
    def test_texturedToUntexturedWithdrawsUVs(self):
        self.setupScene("testUVandUDIM", "testUVs.ma", "pPlaneShape1")
        for src, dst in (("file1.oc", "blinn1.c"), ("file2.oc", "blinn1.ic"),
                         ("file3.oc", "blinn1.sc")):
            cmds.disconnectAttr(src, dst)
        cmds.refresh()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(f="RenderItemPrimvars.TexturedToUntexturedWithdrawsUVs")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
