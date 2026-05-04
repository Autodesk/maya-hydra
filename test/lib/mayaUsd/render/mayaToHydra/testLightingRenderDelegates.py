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
import sys
import time
import tempfile
from contextlib import contextmanager

import maya.cmds as cmds

# Ensure testUtils (with imageUtils) is on path *before* mtohUtils imports imageUtils.
# Otherwise PYTHONPATH order (maya-usd install first) may load a different imageUtils.
_test_utils = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "..", "..", "..", "testUtils"
))
if os.path.isdir(_test_utils) and _test_utils not in sys.path:
    sys.path.insert(0, _test_utils)


def _log(msg):
    """Write to original stdout so CI logs capture it (Maya may redirect sys.stdout)."""
    sys.__stdout__.write(msg + "\n")
    sys.__stdout__.flush()


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
        "convergenceTimeout": 15,  # Wait up to 15s for convergence before snapshot
    },
]


class TestLightingRenderDelegates(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = []

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 7.0  # Images are non-deterministic for shadows even with Storm.
    IMAGE_DIFF_FAIL_PERCENT_PRMAN = 10.0  # PRMan renders are noisier; relax threshold.
    IMAGE_DIFF_FAIL_PERCENT_COVERAGE = 10.0

    @classmethod
    def setUpClass(cls):
        """Prime PRMan/USD search paths and enable useful logging early.

        Some components may snapshot environment variables during initialization,
        so we set key vars once at class setup time (before any renders).

        - Maya Script Editor history: capture delegate output and dump to CI logs
        - RenderMan config override: bump verbosity via a minimal rendermn.ini
        """
        super(TestLightingRenderDelegates, cls).setUpClass()

        scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
        sceneDir = os.path.dirname(os.path.abspath(scenePath))

        # Save env we will modify.
        cls._saved_env = {
            "RMAN_CONFIG_OVERRIDE": os.environ.get("RMAN_CONFIG_OVERRIDE"),
            "RMAN_DUMP_DEFAULTS": os.environ.get("RMAN_DUMP_DEFAULTS"),
            "RDIR": os.environ.get("RDIR"),
        }

        # RenderMan logging knobs (best-effort; harmless if ignored).
        log_root = os.path.join(tempfile.gettempdir(), "mayaHydra_prman_logs")
        os.makedirs(log_root, exist_ok=True)

        # Minimal override file to bump prman log verbosity.
        ini_path = os.path.join(log_root, "rendermn.ini")
        try:
            with open(ini_path, "w", encoding="utf-8") as f:
                f.write("loglevel 4\n")
        except Exception as e:
            _log("Warning: failed to write rendermn.ini override: {}".format(e))

        os.environ["RMAN_CONFIG_OVERRIDE"] = log_root
        os.environ["RMAN_DUMP_DEFAULTS"] = "1"
        # RDIR: alternate config dir (some RenderMan versions use this).
        os.environ["RDIR"] = log_root

        _log(
            "PRMan setUpClass: sceneDir={} | RMAN_CONFIG_OVERRIDE={} | RDIR={} | RMAN_SHADERPATH={} | PRMAN_DELEGATE_PLUGIN_PATH={} | MayaHistory={}".format(
                sceneDir,
                os.environ.get("RMAN_CONFIG_OVERRIDE", ""),
                os.environ.get("RDIR", ""),
                os.environ.get("RMAN_SHADERPATH", ""),
                os.environ.get("PRMAN_DELEGATE_PLUGIN_PATH", ""),
                getattr(cls, "_script_editor_history_file", None),
            )
        )

    @classmethod
    def tearDownClass(cls):
        # Restore environment variables.
        saved_env = getattr(cls, "_saved_env", None) or {}
        for k, v in saved_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v

        # MayaHydraBaseTestCase.tearDownClass runs cmds.file(new=True, force=True) and
        # unloads plugins; that can hang indefinitely after a failed PRMan test. Coverage
        # runs use fixturesUtils.runTests(..., coverage_quit_workaround=True) and os._exit,
        # so skipping base teardown is safe for this single-class module.
        if os.environ.get("MAYAHYDRA_CODE_COVERAGE"):
            _log(
                "LightingRenderDelegates: skipping MayaHydraBaseTestCase.tearDownClass "
                "(file new / unloadPlugin can hang after PRMan failure)"
            )
            return

        super(TestLightingRenderDelegates, cls).tearDownClass()

    @contextmanager
    def _prmanContext(self):
        """Per-render context for PRMan: set Maya workspace to scene dir (no chdir, so snapshots stay in output folder), dump Script Editor on exit."""
        saved_workspace = cmds.workspace(q=True, rd=True)

        try:
            scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
            sceneDir = os.path.dirname(os.path.abspath(scenePath))

            cmds.workspace(sceneDir, o=True)

            yield
        finally:
            # Restore Maya workspace.
            try:
                cmds.workspace(saved_workspace, o=True)
            except Exception as e:
                _log("Warning: failed to restore Maya workspace: {}".format(e))

    def _log_prman_diagnostics(self, stage):
        """Log PRMan-related diagnostics to help debug delegate init failures."""
        _log("PRMan diagnostics [{}]".format(stage))
        keys = [
            "MAYAHYDRA_CODE_COVERAGE",
            "RMANTREE",
            "RENDERMAN_LOCATION",
            "PRMAN_DELEGATE_PLUGIN_PATH",
            "PIXAR_LICENSE_FILE",
            "RMAN_SHADERPATH",
            "RMAN_CONFIG_OVERRIDE",
            "RDIR",
            "RMAN_DUMP_DEFAULTS",
            "CUDA_VISIBLE_DEVICES",
            "CUDA_DEVICE_ORDER",
            "NV_GPU",
        ]
        for key in keys:
            _log("  {}={}".format(key, os.environ.get(key, "")))

        # Inspect delegate plugin path entries.
        plugin_path = os.environ.get("PRMAN_DELEGATE_PLUGIN_PATH", "")
        if plugin_path:
            sep = ";" if platform.system() == "Windows" else ":"
            for entry in [p for p in plugin_path.split(sep) if p]:
                _log("  PRMAN_DELEGATE_PLUGIN_PATH entry: {} (exists={})".format(
                    entry, os.path.isdir(entry)))
                plug_info = os.path.join(entry, "plugInfo.json")
                _log("    plugInfo.json: {} (exists={})".format(
                    plug_info, os.path.isfile(plug_info)))

        # Report renderer/override registration state.
        try:
            _log("  mayaHydra listRenderers: {}".format(cmds.mayaHydra(listRenderers=True)))
        except Exception as e:
            _log("  mayaHydra listRenderers failed: {}".format(e))
        try:
            _log("  mayaHydra listActiveRenderers: {}".format(
                cmds.mayaHydra(listActiveRenderers=True)))
        except Exception as e:
            _log("  mayaHydra listActiveRenderers failed: {}".format(e))
        try:
            _log("  mayaHydra listRegisteredOverrides: {}".format(
                cmds.mayaHydra(listRegisteredOverrides=True)))
        except Exception as e:
            _log("  mayaHydra listRegisteredOverrides failed: {}".format(e))

    def _setRenderer(self, delegate):
        """Switch the viewport to the given Hydra renderer."""
        if delegate.get("name") == "PRMan":
            self._log_prman_diagnostics("before renderer switch")
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, rendererOverrideName=delegate["override"])
        cmds.refresh(force=True)

        # Validate via mayaHydra (modelEditor query can be unreliable when MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1)
        activeRenderers = cmds.mayaHydra(listActiveRenderers=True)
        if delegate.get("name") == "PRMan" and delegate["plugin"] not in activeRenderers:
            self._log_prman_diagnostics("after renderer switch - not active")
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
        If the renderer does not report convergence within the timeout (e.g. PRMan in interactive
        mode), we proceed anyway and take the snapshot."""
        timeoutSeconds = delegate.get("convergenceTimeout", 0)
        if timeoutSeconds <= 0:
            _log("Convergence wait skipped for {} (timeoutSeconds={})".format(
                delegate.get("name"), timeoutSeconds))
            return
        rendererPlugin = delegate["plugin"]
        start = time.time()
        last_log = 0
        while (time.time() - start) < timeoutSeconds:
            cmds.refresh(force=True)
            if cmds.mayaHydraTesting(converged=True, rendererName=rendererPlugin):
                cmds.refresh(force=True)
                _log("Convergence reached for {} after {:.2f}s".format(
                    delegate.get("name"), time.time() - start))
                return
            elapsed = time.time() - start
            if elapsed - last_log >= 5:
                last_log = elapsed
                _log("Waiting for convergence: {} {:.1f}s/{}s".format(
                    delegate.get("name"), elapsed, timeoutSeconds))
            time.sleep(0.5)
        # Timeout reached; some renderers (e.g. PRMan) never report convergence in interactive mode.
        _log("Convergence timeout for {} after {:.2f}s (continuing)".format(
            delegate.get("name"), time.time() - start))
        cmds.refresh(force=True)

    def _setLightIntensities(self, activeLight, intensity):
        """Set all lights to 0 except activeLight, which gets the given intensity."""
        for lightName in LIGHTS:
            shape = self._getLightShape(lightName)
            value = intensity if lightName == activeLight else 0.0
            cmds.setAttr("{}.intensity".format(shape), value)

    def _runLightSnapshots(self, delegate):
        """For each light, enable it alone, take a snapshot, and compare to baseline.

        Compare immediately after each capture. Uses assertSnapshotClose so that on
        failure, Baseline/Actual/Diff/Browse links work when JENKINS_ARTIFACT_BASE
        and JENKINS_ARTIFACT_WORKSPACE (or WORKSPACE) are set in CI."""
        self._setRenderer(delegate)
        fail, failpercent = self._getImageDiffThresholds(delegate)
        for lightName in LIGHTS:
            _log("LightingRenderDelegates: {} -> {}".format(delegate.get("name"), lightName))
            intensity = self._getIntensityForLight(lightName, delegate)
            self._setLightIntensities(lightName, intensity)
            cmds.refresh(force=True)
            self._waitForConvergence(delegate)
            baselineName = "{}_{}.png".format(delegate["name"], lightName)
            self.assertSnapshotClose(
                baselineName,
                fail,
                failpercent,
            )
        _log("LightingRenderDelegates: {} completed".format(delegate.get("name")))

    def _getImageDiffThresholds(self, delegate):
        """Allow higher tolerance for PRMan (noisier renders) and coverage builds."""
        fail = self.IMAGE_DIFF_FAIL_THRESHOLD
        failpercent = self.IMAGE_DIFF_FAIL_PERCENT
        if delegate.get("name") == "PRMan":
            failpercent = max(failpercent, self.IMAGE_DIFF_FAIL_PERCENT_PRMAN)
        if os.environ.get("MAYAHYDRA_CODE_COVERAGE"):
            failpercent = max(failpercent, self.IMAGE_DIFF_FAIL_PERCENT_COVERAGE)
        return fail, failpercent

    def _delegateRunsOnPlatform(self, delegate):
        """Return True if the delegate's platform restriction allows running on the current platform."""
        required = delegate.get("platform", "all")
        if required == "all":
            return True
        return platform.system().lower() == required.lower()

    def _get_prman_allowed_platforms(self):
        """Return the configured PRMan allowed platforms from the environment."""
        raw = os.environ.get("MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS", "")
        if not raw:
            return []
        normalized = raw.replace(";", ",")
        platforms = [p.strip().lower() for p in normalized.split(",") if p.strip()]
        return platforms

    def _is_ci_build(self):
        """Return True when running under Jenkins/CI."""
        return bool(os.environ.get("JENKINS_URL") or os.environ.get("BUILD_ID"))

    def _is_prman_platform_allowed(self):
        """Return True if the current platform is allowed for PRMan."""
        allowed = self._get_prman_allowed_platforms()
        return platform.system().lower() in allowed

    def _is_prman_renderer_available(self):
        """Return True if PRMan renderer is available via mayaHydra."""
        try:
            renderers = cmds.mayaHydra(listRenderers=True) or []
            self._prman_renderer_list = renderers
        except Exception as e:
            _log("PRMan availability check failed: {}".format(e))
            self._prman_renderer_list = []
            return False
        return "HdPrmanLoaderRendererPlugin" in renderers

    def _should_run_prman_delegate(self):
        """Return True if PRMan delegate should run in this environment."""
        allowed = self._get_prman_allowed_platforms()
        if not allowed:
            msg = "PRMan delegate skipped: MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS not set."
            # On Windows CI, PRMan is expected to be installed.
            if self._is_ci_build() and platform.system().lower() == "windows":
                self.fail("PRMan delegate required on CI but {}"
                          .format(msg.replace("skipped: ", "")))
            _log(msg)
            return False
        if not self._is_prman_platform_allowed():
            _log("PRMan delegate skipped: unsupported platform {} (allowed={}).".format(
                platform.system(), ",".join(allowed)))
            return False
        if not self._is_prman_renderer_available():
            renderers = getattr(self, "_prman_renderer_list", [])
            if self._is_ci_build():
                self.fail(
                    "PRMan delegate required on CI but renderer not available. "
                    "listRenderers={}".format(renderers)
                )
            _log("PRMan delegate skipped: renderer not available.")
            return False
        return True

    def loadScene(self):
        """Load the test scene and set the base renderer state."""
        mayaUtils.openTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
        self.setHdStormRenderer()

    def test_EachLight_PerRenderDelegate(self):
        """For each render delegate, enable each Maya light one by one and compare snapshots to baseline."""
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
                if delegate["name"] == "PRMan":
                    if not self._should_run_prman_delegate():
                        continue
                    with self._prmanContext():
                        runDelegate(delegate)
                else:
                    runDelegate(delegate)
        finally:
            # Switch back to Storm before teardown to avoid PRMan/mtoa shutdown
            # hangs when Maya quits (similar to HYDRA-1242 isolate-select issue).
            # Guard cleanup so it doesn't mask the original assertion/error.
            #
            # After a failed image compare under PRMan, modelEditor/refresh can block
            # indefinitely here; unittest never returns and coverage never reaches
            # fixturesUtils.runTests (os._exit). Skip reset when unwinding with an
            # exception in coverage builds — shutdown uses os._exit, so delegate
            # state does not need to be clean.
            try:
                if (
                    os.environ.get("MAYAHYDRA_CODE_COVERAGE")
                    and sys.exc_info()[0] is not None
                ):
                    _log(
                        "LightingRenderDelegates: skipping Storm reset in finally "
                        "(coverage build, unwinding with {})".format(
                            sys.exc_info()[0].__name__
                        )
                    )
                else:
                    self._setRenderer(RENDER_DELEGATES[0])
            except Exception:
                pass


if __name__ == "__main__":
    # Coverage builds can hang during Maya shutdown for this test; force exit with status.
    fixturesUtils.runTests(globals(), coverage_quit_workaround=True)
