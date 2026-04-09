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
# Python test for the generic plugin DAG node translation feature.
# Sets up a Maya scene with two aiPhotometricLight nodes:
#   1. photometricShape1: has non-default attribute values (intensity, aiExposure, aiFilename)
#   2. photometricShapeDefaults1: all attributes at default values
# Then delegates verification to C++ GTest functions via cmds.mayaHydraCppTest,
# passing node paths as positional arguments.
#
import maya.cmds as cmds
import fixturesUtils
import mtohUtils
from testUtils import PluginLoaded


class TestGenericDagNodeTranslation(mtohUtils.MayaHydraBaseTestCase):
    _file = __file__
    _requiredPlugins = ['mtoa']

    def setupScene(self):
        with PluginLoaded('mtoa'):
            xform = cmds.createNode('transform', name='photometric1')
            shape = cmds.createNode(
                'aiPhotometricLight',
                name='photometricShape1',
                parent=xform,
            )
            self._shape = cmds.ls(shape, long=True)[0]
            cmds.setAttr(self._shape + ".intensity", 2.5)
            cmds.setAttr(self._shape + ".aiExposure", 3.0)
            cmds.setAttr(self._shape + ".aiFilename",
                         "/path/to/test.ies", type="string")

            xformDefaults = cmds.createNode('transform',
                                            name='photometricDefaults1')
            shapeDefaults = cmds.createNode(
                'aiPhotometricLight',
                name='photometricShapeDefaults1',
                parent=xformDefaults,
            )
            self._shapeDefaults = cmds.ls(shapeDefaults, long=True)[0]

        cmds.refresh()

    def test_photometricLightTranslation(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shape,
                f="GenericDagNodeTranslation.verifyPrimTypeAndDataSource")

    def test_photometricLightAttributeDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shape,
                f="GenericDagNodeTranslation.verifyAttributeDirtyAndUpdate")

    def test_photometricLightNoDuplicateDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shape,
                f="GenericDagNodeTranslation.verifyNoDuplicateDirtyNotices")

    def test_noSpuriousXformDirty(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shape,
                f="GenericDagNodeTranslation.verifyNoSpuriousXformDirty")

    def test_defaultValuesNotTranslated(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shape,
                f="GenericDagNodeTranslation.verifyDefaultValuesNotTranslated")

    def test_defaultOnlyNodeInHydraWithEmptyAttrs(self):
        self.setupScene()
        with PluginLoaded('mayaHydraCppTests'):
            cmds.mayaHydraCppTest(
                self._shapeDefaults,
                f="GenericDagNodeTranslation.verifyDefaultOnlyNodeInHydraWithEmptyAttrs")


if __name__ == '__main__':
    fixturesUtils.runTests(globals())
