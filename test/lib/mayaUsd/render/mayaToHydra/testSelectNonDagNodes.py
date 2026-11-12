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
import maya.cmds as cmds

import fixturesUtils
import mtohUtils

from pxr import Tf

class TestSelectNonDagNodes(mtohUtils.MayaHydraBaseTestCase):
    '''Selecting Maya nodes outside the DAG (materials, render globals) must not
    report Hydra errors (HYDRA-2303).

    Such a node is never drawn, so it has no Hydra prim. Mapping it to Hydra
    used to fail with a "Mapping for application path ... returned no prim
    path" verification error, from the selection and from the lead object.
    '''
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def _assertNoTfErrors(self, select):
        # Errors posted while a mark is active are held instead of reported.
        mark = Tf.Error.Mark()
        mark.SetMark()
        try:
            select()
            cmds.refresh(force=True)
            errors = [e.commentary for e in mark.GetErrors()]
        finally:
            mark.Clear()
        self.assertEqual(errors, [])

    def test_selectNonDagNodes(self):
        sphere = cmds.polySphere(constructionHistory=False)[0]
        material = cmds.shadingNode('lambert', asShader=True)
        cmds.select(clear=True)
        cmds.refresh(force=True)

        # A material alone, replacing the selection.
        self._assertNoTfErrors(lambda: cmds.select(material, replace=True))

        # A render globals node.
        self._assertNoTfErrors(lambda: cmds.select('defaultRenderGlobals', replace=True))

        # A drawn object, then a material added: the material is the lead.
        self._assertNoTfErrors(lambda: cmds.select(sphere, replace=True))
        self._assertNoTfErrors(lambda: cmds.select(material, add=True))

        # The material removed again: the sphere is the lead.
        self._assertNoTfErrors(lambda: cmds.select(material, deselect=True))
        self.assertEqual(cmds.ls(selection=True), [sphere])

        # Both together, in one replace.
        self._assertNoTfErrors(lambda: cmds.select([material, sphere], replace=True))

        self._assertNoTfErrors(lambda: cmds.select(clear=True))

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
