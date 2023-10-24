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

class TestNamespaces(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    def matchingRprims(self, rprims, matching):
        return len([rprim for rprim in rprims if matching in rprim])

    def test_namespaces(self):
        '''Test that Maya objects in namespaces are supported.'''
        self.setHdStormRenderer()

        # Create a namespace and set it current
        cmds.namespace(add='A')
        cmds.namespace(set='A')

        # Create an object in the namespace
        polyObjs = cmds.polySphere(r=1, sx=20, sy=20, ax=[0, 1, 0], cuv=2 , ch=1)
        self.assertEqual(polyObjs[0], 'A:pSphere1')

        cmds.refresh()

        # There should be two rprims from the poly sphere, one for the mesh and
        # another wireframe for selection highlighting.
        rprims = self.getIndex()
        self.assertEqual(2, len(rprims))

        # Path sanitizing should leave the node name intact.
        self.assertEqual(2, self.matchingRprims(rprims, 'pSphereShape1'))

        # Set the namespace back to the root.
        cmds.namespace(set=':')

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
