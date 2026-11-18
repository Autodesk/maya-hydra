#
# Copyright 2026 Autodesk, Inc. All rights reserved.
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

import os
import sys
import maya.cmds as cmds
import fixturesUtils
import mtohUtils

class TestCustomMaterial(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _requiredPlugins = ['glslShader', 'vertexBufferGenerator', 'mayaHydraCustomShadersNode']

    IMAGE_DIFF_FAIL_THRESHOLD = 0.05
    # macOS GPUs can produce marginally more pixel variance than other platforms
    IMAGE_DIFF_FAIL_PERCENT = 1.5 if sys.platform == 'darwin' else 1.0

    def _customShaderPath(self):
        # This file is <maya-hydra>/test/lib/mayaUsd/render/mayaToHydra/testCustomMaterial.py
        mayaHydraDir = os.path.realpath(os.path.join(
            os.path.dirname(os.path.realpath(__file__)), *[os.pardir] * 5))
        return os.path.join(mayaHydraDir, 'lib', 'flowViewport', 'usdPlugins',
                            'shadersDiscoveryPlugin', 'shaders', 'myCustomShader.ogsfx')

    def test_CustomMaterial(self):
        """
        Assign a Maya GLSLShader using myCustomShader.ogsfx to a sphere, which the
        registered material creation callback replaces by the FVP_CustomBufferShader
        glslfx shader, then render it in textured mode.
        """
        shaderPath = self._customShaderPath()
        self.assertTrue(os.path.isfile(shaderPath),
                        "Custom shader file not found: {}".format(shaderPath))

        cmds.mhRegisterCustomMaterialCallback()
        try:
            sphere = cmds.polySphere()[0]
            shader = cmds.shadingNode("GLSLShader", asShader=True)
            sg = cmds.sets(renderable=True, noSurfaceShader=True, empty=True, name=shader + "SG")
            cmds.connectAttr(shader + ".outColor", sg + ".surfaceShader")
            cmds.sets(sphere, edit=True, forceElement=sg)
            # myCustomShader.ogsfx maps TEXCOORD0 to the custom semantic "myCustomStreamB",
            # so VP2 asks the vertexBufferGenerator plugin to fill that custom vertex buffer.
            # MayaHydra passes it to Hydra as the "myCustomStreamB" primvar, which the
            # FVP_CustomBufferShader replacement reads to compute the sphere color.
            cmds.setAttr(shader + ".shader", shaderPath, type="string")

            self.setTestRenderer()
            self.setBasicCam(3)
            cmds.select(clear=True)

            cmds.modelEditor(self.activeEditor, edit=True,
                             displayAppearance="smoothShaded", displayTextures=True)
            cmds.refresh()

            self.assertSnapshotClose(
                "customMaterial.png",
                self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT
            )
        finally:
            cmds.mhRegisterCustomMaterialCallback(unregister=True)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
