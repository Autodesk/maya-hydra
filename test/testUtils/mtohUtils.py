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
import os
import unittest

import maya.cmds as cmds
import maya.mel

import fixturesUtils
import mayaUtils
import testUtils
from imageUtils import ImageDiffingTestCase
from testUtils import PluginLoaded

import platform
import subprocess
import sys

HD_STORM = "HdStormRendererPlugin"
HD_STORM_OVERRIDE = "mayaHydraRenderOverride_" + HD_STORM
MAYAUSD_PLUGIN_NAME = 'mayaUsdPlugin'

def checkForPlugin(pluginName: str):
    try:
        cmds.loadPlugin(pluginName)
    except:
        return False
    return True

class MayaHydraBaseTestCase(unittest.TestCase, ImageDiffingTestCase):
    '''Base class for mayaHydra unit tests.'''

    DEFAULT_CAM_DIST = 24

    _inputDir = None

    # Variables to be set in subclasses
    _file = None
    _requiredPlugins = []

    @classmethod
    def setUpClass(cls):
        if cls._file is None:
            raise ValueError("Subclasses of MayaHydraBaseTestCase must define "
                             "`_file = __file__`")

        inputPath = fixturesUtils.setUpClass(
            cls._file, 'mayaHydra', initializeStandalone=False, 
            suffix=('_' + cls.__name__))

        if cls._inputDir is None:
            inputDirName = os.path.splitext(os.path.basename(cls._file))[0]
            inputDirName = testUtils.stripPrefix(inputDirName, 'test')
            if not inputDirName.endswith('Test'):
                inputDirName += 'Test'
            cls._inputDir = os.path.join(inputPath, inputDirName)

        cls._testDir = os.path.abspath('.')

        # This optionVar sets the color management status used when creating a new scene.
        # We set it to off to have color management turned off by default before each test
        # (as setUp creates a new file), and to avoid inadvertently turning it on mid-test
        # if a new file is manually created.
        cmds.optionVar(intValue=('colorManagementEnabledByDefault', 0))

        if MAYAUSD_PLUGIN_NAME not in cls._requiredPlugins:
            cls._requiredPlugins.append(MAYAUSD_PLUGIN_NAME)

        for pluginToLoad in cls._requiredPlugins:
            # If a plugin fails to load, the entire test suite will be immediately aborted.
            # Note that in the case of mtoa, the plugin might load successfully but not
            # initialize properly, which means issues will only be caught in the actual tests.
            cmds.loadPlugin(pluginToLoad)
        
    def setUp(self):
        # Maya is not closed/reset between each test of a test suite,
        # so open a new file before each test to minimize leftovers
        # from previous tests.
        mayaUtils.openNewScene()
        self.setHdStormRenderer()

    @classmethod
    def tearDownClass(cls):
        if platform.system() == "Windows":
            # On Windows, ADPClientService can linger around after a test ends and Maya closes,
            # keeping a handle open into the temporary test directory that holds preferences,
            # settings, etc. This prevents us from deleting the temporary test directory, and 
            # thus from cleaning the build. To avoid this, kill the process immediately.
            # So far (2024-03-25), this has only been observed when using LookdevX.
            # Note that the force (/f) flag seems necessary, omitting it did not end up killing
            # the process.
            subprocess.run(['taskkill', '/f', '/im', 'ADPClientService.exe'])

    def setHdStormRenderer(self):
        self.activeEditor = cmds.playblast(activeEditor=1)
        cmds.modelEditor(
            self.activeEditor, e=1,
            rendererOverrideName=HD_STORM_OVERRIDE)
        cmds.refresh(f=1)
        self.delegateId = cmds.mayaHydra(renderer=HD_STORM,
                                    sceneDelegateId="MayaHydraSceneDelegate")
        
    def setViewport2Renderer(self):
        self.activeEditor = cmds.playblast(activeEditor=1)
        # Empty string for rendererOverrideName unsets any currently active override, thus returning to VP2
        cmds.modelEditor(self.activeEditor, e=1, rendererOverrideName="")
        cmds.refresh(f=1)
        self.delegateId = ""

    def setBasicCam(self, dist=DEFAULT_CAM_DIST):
        cmds.setAttr('persp.rotate', -30, 45, 0, type='float3')
        cmds.setAttr('persp.translate', dist, .75 * dist, dist, type='float3')

    def makeCubeScene(self, camDist=DEFAULT_CAM_DIST):
        mayaUtils.openNewScene()
        self.cubeTrans = cmds.polyCube()[0]
        self.cubeShape = cmds.listRelatives(self.cubeTrans)[0]
        self.setHdStormRenderer()
        self.assertNodeNameInIndex(self.cubeShape)
        # The single Maya cube shape maps to two rprims, the first once of
        # which is the shape's StandardShadedItem.  The list is ordered, as the
        # Hydra call made is HdRenderIndex::GetRprimIds(), which sorts
        # according to std::less<SdfPath>, which will produce
        # lexicographically-ordered paths.
        self.cubeRprim = self.getIndex()[0]
        cmds.select(clear=1)
        cmds.refresh()
        self.assertVisible(self.cubeRprim)
        self.setBasicCam(dist=camDist)
        cmds.select(clear=True)

        # The color and specular roughness of the default standard surface changed, set
        # them back to the old default value so the tests keep on working correctly.
        if maya.mel.eval("defaultShaderName") == "standardSurface1":
            color = (0.8, 0.8, 0.8)
            cmds.setAttr("standardSurface1.baseColor", type='float3', *color)
            cmds.setAttr("standardSurface1.specularRoughness", 0.4)

    def getIndex(self, **kwargs):
        return cmds.mayaHydra(renderer=HD_STORM, listRenderIndex=True, **kwargs)

    def getVisibleIndex(self, **kwargs):
        kwargs['visibleOnly'] = True
        return self.getIndex(**kwargs)

    def assertVisible(self, rprim):
        self.assertIn(rprim, self.getVisibleIndex())

    def assertInIndex(self, rprim):
        self.assertIn(rprim, self.getIndex())

    def assertNodeNameInIndex(self, nodeName):
        for rprim in self.getIndex():
            if nodeName in rprim:
                return True
        return False

    def trace(self, msg):
        sys.__stdout__.write(msg)
        sys.__stdout__.flush()

    def traceIndex(self, msg):
        self.trace(msg.format(str(self.getIndex())))

    def resolveRefImage(self, refImage, imageVersion):
        if not os.path.isabs(refImage):
            if imageVersion:
                refImage = os.path.join(self._inputDir, imageVersion, refImage)
            else:
                refImage = os.path.join(self._inputDir, refImage)
        return refImage

    def assertImagesClose(self, image1, image2, fail, failpercent, image1Version=None, image2Version=None, 
                hardfail=None, warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        imagePath1 = self.resolveRefImage(image1, image1Version)
        imagePath2 = self.resolveRefImage(image2, image2Version)
        super(MayaHydraBaseTestCase, self).assertImagesClose(imagePath1, imagePath2, fail, failpercent, hardfail, 
                            warn, warnpercent, hardwarn, perceptual)
        
    def assertImagesEqual(self, image1, image2, image1Version=None, image2Version=None):
        imagePath1 = self.resolveRefImage(image1, image1Version)
        imagePath2 = self.resolveRefImage(image2, image2Version)
        super(MayaHydraBaseTestCase, self).assertImagesEqual(imagePath1, imagePath2)

    def assertSnapshotClose(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        super(MayaHydraBaseTestCase, self).assertSnapshotClose(refImagePath, fail, failpercent, hardfail,
                            warn, warnpercent, hardwarn, perceptual)

    def assertSnapshotEqual(self, refImage, imageVersion=None):
        '''Use of this method is discouraged, as renders can vary slightly between renderer architectures.'''
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        super(MayaHydraBaseTestCase, self).assertSnapshotEqual(refImagePath)
    
    def assertSnapshotSilhouetteClose(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        super(MayaHydraBaseTestCase, self).assertSnapshotSilhouetteClose(refImagePath, fail, failpercent, hardfail,
                            warn, warnpercent, hardwarn, perceptual)

    def assertSnapshotAndCompareVp2(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        self.setHdStormRenderer()
        self.assertSnapshotClose(refImage, fail, failpercent, imageVersion, hardfail, warn, warnpercent, hardwarn, perceptual)

        self.setViewport2Renderer()
        self.assertSnapshotSilhouetteClose(refImage, fail, failpercent, imageVersion, hardfail, warn, warnpercent, hardwarn, perceptual)

        self.setHdStormRenderer()

    def runCppTest(self, testFilter):
        with PluginLoaded("mayaHydraCppTests"):
            cmds.mayaHydraCppTest(f=testFilter, inputDir=self._inputDir, outputDir=self._testDir)
