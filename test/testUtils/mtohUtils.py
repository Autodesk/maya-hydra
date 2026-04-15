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
import platform
import unittest

import maya.cmds as cmds
import maya.mel

import fixturesUtils
import mayaUtils
import testUtils
from imageUtils import ImageDiffingTestCase
from testUtils import PluginLoaded
from pxr import Usd

import subprocess
import sys
import math

HD_STORM = "HdStormRendererPlugin"
HD_STORM_OVERRIDE = "mayaHydraRenderOverride_" + HD_STORM
MAYAUSD_PLUGIN_NAME = 'mayaUsdPlugin'

class MayaHydraBaseTestCase(unittest.TestCase, ImageDiffingTestCase):
    '''Base class for mayaHydra unit tests.'''

    DEFAULT_CAM_DIST = 24

    _inputDir = None

    # Variables to be set in subclasses
    _file = None
    _requiredPlugins = []
    _pluginsToUnload = []

    #The OpenUSD version
    _usdVersion = None
    _usdEnvLogged = False

    # Unloading mayaHydraFlowViewportAPILocator crashes Maya (HYDRA-1304).
    # Unloading mtoa succeeds on Linux, but fails on Windows and macOS
    # with "cannot be unloaded because it is still in use" error.
    # Unloading modelingToolkit fails with a
    # "Dynamic unloading is not currently supported." error
    # mayaUsdPlugin looged as HYDRA-1896, we should remove this when HYDRA-1896 is fixed
    _pluginsCantUnload = ['mayaHydraFlowViewportAPILocator', 'mtoa', 'modelingToolkit', 'mayaUsdPlugin']

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

        for p in cls._requiredPlugins:
            # If a plugin fails to load, the entire test suite will be immediately aborted.
            # Note that in the case of mtoa, the plugin might load successfully but not
            # initialize properly, which means issues will only be caught in the actual tests.
            if not cmds.pluginInfo(p, q=True, loaded=True):
                cls._pluginsToUnload.append(p)
                cmds.loadPlugin(p, quiet=True)

        # Set the usd version
        cls._usdVersion = Usd.GetVersion()
        cls._logUsdEnvironment()

        # Enable Script Editor history capture for image diff debugging.
        cls._enable_script_editor_history_capture()

    @classmethod
    def _enable_script_editor_history_capture(cls):
        """Capture Script Editor output to a file for debugging diffs."""
        try:
            cls._saved_script_editor = {
                "writeHistory": cmds.scriptEditorInfo(q=True, writeHistory=True),
                "historyFilename": cmds.scriptEditorInfo(q=True, historyFilename=True),
            }
            history_file = os.path.join(cls._testDir, "maya_scriptEditor_history.txt")
            cmds.scriptEditorInfo(edit=True, writeHistory=True, historyFilename=history_file)
            cls._script_editor_history_file = history_file
        except Exception:
            cls._saved_script_editor = None
            cls._script_editor_history_file = None

    @classmethod
    def _restore_script_editor_history_capture(cls):
        """Restore Script Editor capture settings."""
        try:
            saved = getattr(cls, "_saved_script_editor", None)
            if saved:
                cmds.scriptEditorInfo(
                    edit=True,
                    writeHistory=saved.get("writeHistory", False),
                    historyFilename=saved.get("historyFilename", ""),
                )
        except Exception:
            pass

    def _dump_script_editor_history(self, title):
        """Dump Script Editor history to stdout for CI visibility."""
        if getattr(self, "_script_editor_dumped", False):
            return
        path = getattr(self.__class__, "_script_editor_history_file", None)
        if not path or not os.path.isfile(path):
            return
        self._script_editor_dumped = True
        try:
            sys.__stdout__.write("\n===== BEGIN {} ({}) =====\n".format(title, path))
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                for ln in f:
                    sys.__stdout__.write(ln)
            sys.__stdout__.write("===== END {} =====\n".format(title))
            sys.__stdout__.flush()
        except Exception:
            pass

    @classmethod
    def _logUsdEnvironment(cls):
        """Log USD-related environment details for debugging."""
        if cls._usdEnvLogged:
            return
        cls._usdEnvLogged = True
        keys = [
            "PXR_USD_LOCATION",
            "USD_INSTALL_LOCATION",
            "PXR_PLUGINPATH_NAME",
            "MAYA_PXR_PLUGINPATH_NAME",
            "PXR_OVERRIDE_PLUGINPATH_NAME",
            "LD_LIBRARY_PATH",
            "DYLD_LIBRARY_PATH",
            "TF_DEBUG",
            "RMANTREE",
            "PRMAN_DELEGATE_PLUGIN_PATH",
        ]
        env_parts = []
        for key in keys:
            env_parts.append("{}={}".format(key, os.environ.get(key, "")))
        sys.__stdout__.write("USD env: {}\n".format(" | ".join(env_parts)))
        sys.__stdout__.write("USD version: {}\n".format(cls._usdVersion))
        try:
            sep = ";" if platform.system() == "Windows" else ":"
            plugin_path = os.environ.get("PXR_PLUGINPATH_NAME", "")
            plugin_entries = [p for p in plugin_path.split(sep) if p]
            sys.__stdout__.write("USD plugin path entries ({}):\n".format(len(plugin_entries)))
            for idx, p in enumerate(plugin_entries):
                sys.__stdout__.write("  {}: {}\n".format(idx, p))
        except Exception as e:
            sys.__stdout__.write("USD plugin path entries: (unavailable) {}\n".format(e))
        plugin_path = None
        try:
            plugin_path = cmds.pluginInfo(MAYAUSD_PLUGIN_NAME, q=True, path=True)
            sys.__stdout__.write("mayaUsdPlugin path: {}\n".format(plugin_path))
        except Exception as e:
            sys.__stdout__.write("mayaUsdPlugin path: (unavailable) {}\n".format(e))
        try:
            from pxr import Plug

            registry = Plug.Registry()
            plugins = registry.GetAllPlugins()
            sys.__stdout__.write("USD plugins discovered: {}\n".format(len(plugins)))

            name_to_paths = {}
            for plugin in plugins:
                name = getattr(plugin, "name", None)
                if not name and hasattr(plugin, "GetName"):
                    name = plugin.GetName()
                path = getattr(plugin, "path", None)
                if not path and hasattr(plugin, "GetPath"):
                    path = plugin.GetPath()
                if not name:
                    continue
                name_to_paths.setdefault(name, []).append(path or "")

            dup_names = {
                name: paths for name, paths in name_to_paths.items()
                if len(set([p for p in paths if p])) > 1
            }
            if dup_names:
                sys.__stdout__.write("USD plugin duplicates detected:\n")
                for name in sorted(dup_names.keys()):
                    sys.__stdout__.write("  {}\n".format(name))
                    for p in dup_names[name]:
                        if p:
                            sys.__stdout__.write("    {}\n".format(p))
            else:
                sys.__stdout__.write("USD plugin duplicates detected: none\n")
        except Exception as e:
            sys.__stdout__.write("USD plugin registry logging failed: {}\n".format(e))
        try:
            if platform.system() == "Darwin":
                def _print_rpaths(label, path):
                    if not path or not os.path.isfile(path):
                        sys.__stdout__.write("{} rpath: (missing) {}\n".format(label, path))
                        return
                    try:
                        output = subprocess.check_output(
                            ["otool", "-l", path],
                            stderr=subprocess.STDOUT,
                            text=True,
                        )
                        lines = []
                        capture = False
                        for line in output.splitlines():
                            if "cmd LC_RPATH" in line:
                                capture = True
                                lines.append(line.strip())
                            elif capture and "path " in line:
                                lines.append(line.strip())
                                capture = False
                        sys.__stdout__.write("{} rpath:\n".format(label))
                        for ln in lines:
                            sys.__stdout__.write("  {}\n".format(ln))
                    except Exception as err:
                        sys.__stdout__.write("{} rpath: (error) {}\n".format(label, err))

                if plugin_path:
                    _print_rpaths("mayaUsdPlugin", plugin_path)
                try:
                    mtoa_path = cmds.pluginInfo("mtoa", q=True, path=True)
                    _print_rpaths("mtoa", mtoa_path)
                except Exception:
                    pass
        except Exception as e:
            sys.__stdout__.write("rpath dump failed: {}\n".format(e))
        sys.__stdout__.flush()
        
    def setUp(self):
        self._script_editor_dumped = False
        # Enable Maya Script Editor history capture for all tests (dumped on image failure).
        try:
            prev_write = cmds.scriptEditorInfo(q=True, writeHistory=True)
            prev_file = cmds.scriptEditorInfo(q=True, historyFilename=True) or ""
            self._saved_script_editor = {"writeHistory": prev_write, "historyFilename": prev_file}
            if not prev_write:
                history_file = os.path.join(self._testDir, "maya_scriptEditor_history.txt")
                cmds.scriptEditorInfo(edit=True, writeHistory=True, historyFilename=history_file)
        except Exception:
            self._saved_script_editor = None

        # Maya is not closed/reset between each test of a test suite,
        # so open a new file before each test to minimize leftovers
        # from previous tests.
        mayaUtils.openNewScene()
        modified = cmds.file(query=True, modified=True)
        assert not modified, 'Internal test framework error: scene left as modified by mayaUtils.openNewScene()'

        self.setHdStormRenderer()

        # We've just opened a new scene, so we should not be modified.  Setting
        # Storm as the renderer should conceptually not change that status, but
        # unfortunately in automated tests it does (see setHdStormRender()
        # method documentation).  Restore modified status to false.
        cmds.file(modified=False)

    def tearDown(self):
        # Restore Script Editor capture if we enabled it in setUp.
        try:
            saved = getattr(self, "_saved_script_editor", None)
            if saved and not saved.get("writeHistory", True):
                cmds.scriptEditorInfo(
                    edit=True,
                    writeHistory=saved["writeHistory"],
                    historyFilename=saved.get("historyFilename", ""),
                )
        except Exception:
            pass

    @classmethod
    def tearDownClass(cls):
        cls._restore_script_editor_history_capture()
        # Clean out the scene to allow all plugins to unload cleanly.
        cmds.file(new=True, force=True)
        for p in reversed(cls._pluginsToUnload):
            if p not in cls._pluginsCantUnload:
                cmds.unloadPlugin(p)

        if platform.system() == "Windows":
            # On Windows, ADPClientService can linger around after a test ends and Maya closes,
            # keeping a handle open into the temporary test directory that holds preferences,
            # settings, etc. This prevents us from deleting the temporary test directory, and
            # thus from cleaning the build. To avoid this, kill the process immediately.
            # So far (2024-03-25), this has only been observed when using LookdevX.
            # Note that the force (/f) flag seems necessary, omitting it did not end up killing
            # the process. For hard-exiting the Maya host itself (e.g. coverage), see
            # fixturesUtils.runTests.
            subprocess.run(['taskkill', '/f', '/im', 'ADPClientService.exe'])

    def setHdStormRenderer(self):
        self.activeEditor = cmds.playblast(activeEditor=1)
        cmds.modelEditor(
            self.activeEditor, e=1,
            rendererOverrideName=HD_STORM_OVERRIDE)
        # During automated tests, tracing demonstrates that the following call
        # to refresh marks the scene as modified, with the modified node being
        # defaultRenderGlobals.  This behavior cannot be reproduced in a
        # non-automated interactive Maya.
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

    def setMayaDefaultLightIntensity(self, intensity):
        if maya.mel.eval("optionVar -exists defaultLightIntensity"):
            maya.mel.eval("optionVar -fv defaultLightIntensity {}".format(intensity))
        if cmds.attributeQuery('defaultLightIntensity', node='hardwareRenderingGlobals', exists=True):
            cmds.setAttr('hardwareRenderingGlobals.defaultLightIntensity', intensity)

    def modifyDefaultLightIntensityByUsdVersion(self):
        #Add any new case here
        if self._usdVersion >= (0, 24, 11):
            #For Usd 24.11+ set the default light intensity to PI instead of 1.0 to counter balance the changes in usd 24.11
            #The original setUp() function sets the default light to 1.0
            self.setMayaDefaultLightIntensity(math.pi)
        
    def resetDefaultLightIntensityByUsdVersion(self):
        #Add any case for here
        if self._usdVersion >= (0, 24, 11):
            #For Usd 24.11+ reset the default light intensity to 1 instead of PI to counter balance the changes in usd 24.11
            self.setMayaDefaultLightIntensity(1.0)

    def makeCubeScene(self, camDist=DEFAULT_CAM_DIST):
        mayaUtils.openNewScene()
        self.cubeTrans = cmds.polyCube()[0]
        self.cubeShape = cmds.listRelatives(self.cubeTrans)[0]
        self.setHdStormRenderer()
        self.assertNodeNameInIndex(self.cubeShape)
        index_list = self.getIndex()
        # Get the cube prim by ignoring the prims whose name contains DormantPolywire
        cubePrims = [p for p in index_list if 'dormantpolywire' not in p.lower()]
        if cubePrims:
            self.cubeRprim = cubePrims[0]
        else:
            self.fail("Expected a non-DormantPolyWire prim, but none was found")
        
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
                return
        raise AssertionError(nodeName + ' not in index')

    def assertNodeNameNotInIndex(self, nodeName):
        for rprim in self.getIndex():
            if nodeName in rprim:
                raise AssertionError(nodeName + ' in index')
        return

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
                hardfail=None, failrelative=None, warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        imagePath1 = self.resolveRefImage(image1, image1Version)
        imagePath2 = self.resolveRefImage(image2, image2Version)
        try:
            super(MayaHydraBaseTestCase, self).assertImagesClose(
                imagePath1, imagePath2, fail, failpercent, hardfail=hardfail,
                failrelative=failrelative, warn=warn, warnpercent=warnpercent,
                hardwarn=hardwarn, perceptual=perceptual)
        except AssertionError:
            self._dump_script_editor_history("Maya Script Editor history (image diff)")
            raise
        
    def assertImagesEqual(self, image1, image2, image1Version=None, image2Version=None):
        imagePath1 = self.resolveRefImage(image1, image1Version)
        imagePath2 = self.resolveRefImage(image2, image2Version)
        try:
            super(MayaHydraBaseTestCase, self).assertImagesEqual(imagePath1, imagePath2)
        except AssertionError:
            self._dump_script_editor_history("Maya Script Editor history (image diff)")
            raise

    def assertSnapshotClose(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                failrelative=None, warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        try:
            super(MayaHydraBaseTestCase, self).assertSnapshotClose(
                refImagePath, fail, failpercent, hardfail=hardfail,
                failrelative=failrelative, warn=warn, warnpercent=warnpercent,
                hardwarn=hardwarn, perceptual=perceptual, imageVersion=imageVersion)
        except AssertionError:
            self._dump_script_editor_history("Maya Script Editor history (image diff)")
            raise

    def assertSnapshotEqual(self, refImage, imageVersion=None):
        '''Use of this method is discouraged, as renders can vary slightly between renderer architectures.'''
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        try:
            super(MayaHydraBaseTestCase, self).assertSnapshotEqual(refImagePath)
        except AssertionError:
            self._dump_script_editor_history("Maya Script Editor history (image diff)")
            raise
    
    def assertSnapshotSilhouetteClose(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                failrelative=None, warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        refImagePath = self.resolveRefImage(refImage, imageVersion)
        try:
            super(MayaHydraBaseTestCase, self).assertSnapshotSilhouetteClose(
                refImagePath, fail, failpercent, hardfail=hardfail,
                failrelative=failrelative, warn=warn, warnpercent=warnpercent,
                hardwarn=hardwarn, perceptual=perceptual)
        except AssertionError:
            self._dump_script_editor_history("Maya Script Editor history (image diff)")
            raise

    def assertSnapshotAndCompareVp2(self, refImage, fail, failpercent, imageVersion=None, hardfail=None, 
                failrelative=None, warn=None, warnpercent=None, hardwarn=None, perceptual=False):
        self.setHdStormRenderer()
        self.assertSnapshotClose(refImage, fail, failpercent, imageVersion, hardfail=hardfail,
            failrelative=failrelative, warn=warn, warnpercent=warnpercent, hardwarn=hardwarn, perceptual=perceptual)

        self.setViewport2Renderer()
        self.assertSnapshotSilhouetteClose(refImage, fail, failpercent, imageVersion, hardfail=hardfail,
            failrelative=failrelative, warn=warn, warnpercent=warnpercent, hardwarn=hardwarn, perceptual=perceptual)

        self.setHdStormRenderer()

    def runCppTest(self, testFilter):
        with PluginLoaded("mayaHydraCppTests"):
            cmds.mayaHydraCppTest(f=testFilter, inputDir=self._inputDir, outputDir=self._testDir)
