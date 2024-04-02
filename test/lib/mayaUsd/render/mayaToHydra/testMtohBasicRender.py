# Copyright 2020 Luma Pictures
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
# Test to make sure that our snapshot-comparison tools work
import maya.cmds as cmds
import maya.mel

import fixturesUtils
import mtohUtils

class TestSnapshot(mtohUtils.MayaHydraBaseTestCase):
    """Tests whether our snapshot rendering works with basic Viewport 2.0"""

    _file = __file__

    def test_flat_orange(self):
        activeEditor = cmds.playblast(activeEditor=1)

        # Note that we use the default viewport2 renderer, because we're not testing
        # whether hdmaya works with this test - just whether we can make a snapshot

        cmds.modelEditor(
            activeEditor, e=1,
            rendererName='vp2Renderer')
        cmds.modelEditor(
            activeEditor, e=1,
            rendererOverrideName="")

        cube = cmds.polyCube()[0]
        shader = cmds.shadingNode("surfaceShader", asShader=1)
        cmds.select(cube)
        cmds.hyperShade(assign=shader)

        COLOR = (.75, .5, .25)
        cmds.setAttr('{}.outColor'.format(shader), type='float3', *COLOR)

        cmds.setAttr('persp.rotate', 0, 0, 0, type='float3')
        cmds.setAttr('persp.translate', 0, .25, .7, type='float3')

        self.assertSnapshotEqual("flat_orange.png")
        self.assertRaises(AssertionError,
                          self.assertSnapshotEqual, "flat_orange_bad.png")        

class TestMayaHydraRender(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__

    def test_cube(self):
        imageVersion = None
        if maya.mel.eval("defaultShaderName") != "standardSurface1":
            imageVersion = 'lambertDefaultMaterial'

        self.makeCubeScene(camDist=6)
        self.assertSnapshotEqual("cube_unselected.png", imageVersion)
        cmds.select(self.cubeTrans)
        self.assertSnapshotEqual("cube_selected.png", imageVersion)


if __name__ == '__main__':
    fixturesUtils.runTests(globals())

