# Copyright 2025 Autodesk
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

import os

class WriteFile(object):
    '''
    Context manager to write a render to a file with a specified resolution.
    '''
    def __init__(self, outputFile, width, height):
        self.outputFile = outputFile
        self.width      = width
        self.height     = height

    def __enter__(self):
        '''Returns True.'''
        cmds.mayaHydraCppTest(self.outputFile, f="TestWriteFile.setFileName")
        cmds.mayaHydraCppTest(self.width, self.height, f="TestWriteFile.setImageSize")
        return True

    def __exit__(self, exc_type, exc_value, traceback):
        cmds.mayaHydraCppTest(f="TestWriteFile.unsetImageSize")
        cmds.mayaHydraCppTest("", f="TestWriteFile.setFileName")


class TestWriteFile(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__

    _requiredPlugins = ['mayaHydraCppTests']

    def test_WriteFileStorm(self):

        outputFile = os.path.join(self.getSnapshotDir(), 'output.png')

        cmds.polyTorus(r=1, sr=0.5, tw=0, sx=20, sy=20, ax=[0, 1, 0], cuv=1, ch=1)

        with WriteFile(outputFile, 1280, 720):
            cmds.refresh()

        self.assertImagesClose('stormOutput.png', outputFile, 0.1, 2)

if __name__ == '__main__':
    fixturesUtils.runTests(globals())
