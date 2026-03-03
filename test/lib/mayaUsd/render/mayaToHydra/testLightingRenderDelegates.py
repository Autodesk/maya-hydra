#
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

import os
import platform
import time
from contextlib import contextmanager

import maya.cmds as cmds

import fixturesUtils
import mayaUtils
import mtohUtils
from testUtils import PluginLoaded, getTestScene


# Maya light names in the test scene (transform names; intensity is on the shape).
LIGHTS = ["directionalLight1", "pointLight1", "spotLight1", "areaLight1"]

# Intensity to set for each light when it is the active one.
# Use a dict for per-renderer values.
LIGHT_INTENSITIES = {
    "directionalLight1": {"Storm": 5, "Arnold": 5, "PRMan": 2},
    "pointLight1": {"Storm": 5, "Arnold": 500, "PRMan": 1000},
    "spotLight1": {"Storm": 5, "Arnold": 1000, "PRMan": 1000},
    "areaLight1": {"Storm": 5000, "Arnold": 5000, "PRMan": 5000},
}


# Render delegates: display name, Hydra plugin, override name, Maya plugin to load.
# convergenceTimeout: seconds to wait for progressive renderers before taking a snapshot.
# platform: "all" (default) or "windows" to restrict image comparison to that platform.
RENDER_DELEGATES = [
    {
        "name": "Storm",
        "plugin": mtohUtils.HD_STORM,
        "override": mtohUtils.HD_STORM_OVERRIDE,
        "mayaPlugin": None,
        "convergenceTimeout": 0,  # Storm converges immediately
    },
    # Disabling Arnold render delegate test, logged as HYDRA-2122 and HYDRA-2123
    # {
    #     "name": "Arnold",
    #     "plugin": "HdArnoldRendererPlugin",
    #     "override": "mayaHydraRenderOverride_HdArnoldRendererPlugin",
    #     "mayaPlugin": "mtoa",
    #     "convergenceTimeout": 30,
    # },
    {
        "name": "PRMan",
        "plugin": "HdPrmanLoaderRendererPlugin",
        "override": "mayaHydraRenderOverride_HdPrmanLoaderRendererPlugin",
        "mayaPlugin": None,
        "convergenceTimeout": 30,  # Wait up to 30s for convergence before snapshot
        "platform": "windows",  # Image comparison runs on Windows only
    },
]


class TestLightingRenderDelegates(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = []

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 7.0  # Images are non-deterministic for shadows even with Storm.
    @contextmanager
    def _prmanTexturePath(self):
        """Prepare a robust search context for PRMan on CI/build machines.

        Why this exists:
          - The scene uses relative texture paths (./diffuse.png, ./UVChecker.png).
          - In Jenkins, the process working directory is typically the workspace,
            not the scene directory, so ./... breaks.
          - Depending on how paths flow through Maya->Hydra->hdPrman, relative
            paths may be resolved by:
              * Maya file nodes / workspace (project)
              * the process current working directory
              * USD's default resolver search path (PXR_AR_DEFAULT_SEARCH_PATH)
              * PRMan's own RMAN_TEXTUREPATH

        This context manager makes all of the above consistent for the duration
        of the PRMan render and restores previous state afterwards.
        """
        saved_env = {
            "RMAN_TEXTUREPATH": os.environ.get("RMAN_TEXTUREPATH"),
            "PXR_AR_DEFAULT_SEARCH_PATH": os.environ.get("PXR_AR_DEFAULT_SEARCH_PATH"),
        }
        saved_cwd = os.getcwd()
        saved_workspace = cmds.workspace(q=True, rd=True)

        try:
            scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
            sceneDir = os.path.dirname(os.path.abspath(scenePath))
            sep = os.pathsep  # ';' on Windows, ':' elsewhere

            # 1) Make relative ./... resolve to the scene directory.
            os.chdir(sceneDir)

            # 2) Ensure Maya resolves relative file-node paths via the scene directory.
            cmds.workspace(sceneDir, o=True)

            # 3) Help USD's ArDefaultResolver (if assets flow through as SdfAssetPath).
            prev_ar = saved_env["PXR_AR_DEFAULT_SEARCH_PATH"] or ""
            os.environ["PXR_AR_DEFAULT_SEARCH_PATH"] = sceneDir + (sep + prev_ar if prev_ar else "")

            # 4) Help PRMan find textures if they still arrive as relative paths.
            prev_rman = saved_env["RMAN_TEXTUREPATH"] or ""
            sceneDirNorm = sceneDir.replace("\\", "/")
            os.environ["RMAN_TEXTUREPATH"] = sceneDirNorm + (sep + prev_rman if prev_rman else "")

            # Debug: log current state and verify files exist (helps diagnose CI failures)
            diffusePath = os.path.join(sceneDir, "diffuse.png")
            uvCheckerPath = os.path.join(sceneDir, "UVChecker.png")
            print(
                "PRMan context: cwd={} | workspace={} | RMAN_TEXTUREPATH={} | PXR_AR_DEFAULT_SEARCH_PATH={} | diffuse.png exists={} | UVChecker.png exists={}".format(
                    os.getcwd(),
                    cmds.workspace(q=True, rd=True),
                    os.environ.get("RMAN_TEXTUREPATH", ""),
                    os.environ.get("PXR_AR_DEFAULT_SEARCH_PATH", ""),
                    os.path.isfile(diffusePath),
                    os.path.isfile(uvCheckerPath),
                )
            )

            yield
        finally:
            # Restore Maya workspace and cwd first (some tools read these lazily).
            try:
                cmds.workspace(saved_workspace, o=True)
            except Exception as e:
                print("Warning: failed to restore Maya workspace: {}".format(e))
            try:
                os.chdir(saved_cwd)
            except Exception as e:
                print("Warning: failed to restore cwd: {}".format(e))

            for k, v in saved_env.items():
                if v is None:
                    os.environ.pop(k, None)
                else:
                    os.environ[k] = v


    def loadScene(self):
        # Open the test scene.
        mayaUtils.openTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")

        # Set Maya workspace (project) to the scene directory so that any relative
        # file-node paths like ./diffuse.png resolve correctly in all environments,
        # including CI where the process working directory differs.
        scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
        sceneDir = os.path.dirname(os.path.abspath(scenePath))
        cmds.workspace(sceneDir, o=True)

        cmds.refresh(force=True)
def _setRenderer(self, delegate):
        """Switch the viewport to the given Hydra renderer."""
        panel = cmds.playblast(activeEditor=1)
        cmds.modelEditor(panel, edit=True, rendererOverrideName=delegate["override"])
        cmds.refresh(force=True)

        # Validate via mayaHydra (modelEditor query can be unreliable when MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1)
        activeRenderers = cmds.mayaHydra(listActiveRenderers=True)
        print("Active renderers: {}".format(activeRenderers))
        self.assertIn(
            delegate["plugin"],
            activeRenderers,
            "Renderer switch failed: expected {} in active renderers, got {}".format(
                delegate["plugin"],
                activeRenderers,
            ),
        )

    def _getLightShape(self, lightName):
        """Return the shape node for a Maya light (intensity is on the shape)."""
        shapes = cmds.listRelatives(lightName, shapes=True, noIntermediate=True)
        if not shapes:
            raise ValueError("No shape found for light: {}".format(lightName))
        return shapes[0]

    def _getIntensityForLight(self, lightName, delegate):
        """Resolve intensity for a light given the render delegate."""
        return LIGHT_INTENSITIES[lightName][delegate["name"]]

    def _waitForConvergence(self, delegate):
        """Wait for progressive renderers to converge before taking a snapshot.
        Uses mayaHydraTesting(converged=True). Skips when convergenceTimeout is 0 (e.g. Storm).
        Times out after convergenceTimeout seconds for delegates that may not report convergence.
        """
        timeoutSeconds = delegate.get("convergenceTimeout", 0)
        if timeoutSeconds <= 0:
            return
        rendererPlugin = delegate["plugin"]
        start = time.time()
        while (time.time() - start) < timeoutSeconds:
            cmds.refresh(force=True)
            if cmds.mayaHydraTesting(converged=True, rendererName=rendererPlugin):
                cmds.refresh(force=True)
                return
            time.sleep(0.5)
        # If we exit the loop, the renderer never reported convergence within the timeout.
        cmds.refresh(force=True)
        elapsed = time.time() - start
        rendererName = delegate.get("name", rendererPlugin)
        self.fail(
            "Renderer '{}' ({}) did not report convergence within {:.1f} seconds; "
            "snapshot may be invalid.".format(rendererName, rendererPlugin, elapsed)
        )

    def _setLightIntensities(self, activeLight, intensity):
        """Set all lights to 0 except activeLight, which gets the given intensity."""
        for lightName in LIGHTS:
            shape = self._getLightShape(lightName)
            value = intensity if lightName == activeLight else 0.0
            cmds.setAttr("{}.intensity".format(shape), value)

    def _runLightSnapshots(self, delegate):
        """For each light, enable it alone, take a snapshot, and compare to baseline."""
        self._setRenderer(delegate)
        for lightName in LIGHTS:
            intensity = self._getIntensityForLight(lightName, delegate)
            self._setLightIntensities(lightName, intensity)
            cmds.refresh(force=True)
            self._waitForConvergence(delegate)
            baselineName = "{}_{}.png".format(delegate["name"], lightName)
            self.assertSnapshotClose(
                baselineName,
                self.IMAGE_DIFF_FAIL_THRESHOLD,
                self.IMAGE_DIFF_FAIL_PERCENT,
            )

    def _delegateRunsOnPlatform(self, delegate):
        """Return True if the delegate's platform restriction allows running on the current platform."""
        required = delegate.get("platform", "all")
        if required == "all":
            return True
        return platform.system().lower() == required.lower()

    def test_EachLight_PerRenderDelegate(self):
        """For each render delegate (Storm, Arnold, PRMan), enable each Maya light one by one and compare snapshots to baseline."""
        def runDelegate(delegate):
            self.loadScene()
            mayaPlugin = delegate.get("mayaPlugin")
            if mayaPlugin:
                with PluginLoaded(mayaPlugin):
                    self.assertTrue(
                        cmds.pluginInfo(mayaPlugin, query=True, loaded=True),
                        "{} plugin ({}) failed to load".format(
                            delegate["name"], mayaPlugin
                        ),
                    )
                    self._runLightSnapshots(delegate)
            else:
                self._runLightSnapshots(delegate)

        try:
            for delegate in RENDER_DELEGATES:
                if not self._delegateRunsOnPlatform(delegate):
                    continue
                # Only set RMAN_TEXTUREPATH for PRMan; save/restore to avoid leaking state.
                if delegate["name"] == "PRMan":
                    with self._prmanTexturePath():
                        runDelegate(delegate)
                else:
                    runDelegate(delegate)
        finally:
            # Switch back to Storm before teardown to avoid PRMan/mtoa shutdown
            # hangs when Maya quits (similar to HYDRA-1242 isolate-select issue).
            # Guard cleanup so it doesn't mask the original assertion/error.
            try:
                self._setRenderer(RENDER_DELEGATES[0])
            except Exception:
                pass


if __name__ == "__main__":
    fixturesUtils.runTests(globals())
