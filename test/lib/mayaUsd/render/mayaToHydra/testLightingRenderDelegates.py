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
from imageUtils import snapshot


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
        "platform": "windows",  # Image comparison runs on Windows only
    },
]


class TestLightingRenderDelegates(mtohUtils.MayaHydraBaseTestCase):
    # MayaHydraBaseTestCase.setUpClass requirement.
    _file = __file__
    _requiredPlugins = []

    IMAGE_DIFF_FAIL_THRESHOLD = 0.1
    IMAGE_DIFF_FAIL_PERCENT = 7.0  # Images are non-deterministic for shadows even with Storm.
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

        # Capture Maya Script Editor output (includes many delegate/prman messages).
        try:
            cls._saved_script_editor = {
                "writeHistory": cmds.scriptEditorInfo(q=True, writeHistory=True),
                "historyFilename": cmds.scriptEditorInfo(q=True, historyFilename=True),
            }
            history_file = os.path.join(log_root, "maya_scriptEditor_history.txt")
            cmds.scriptEditorInfo(edit=True, writeHistory=True, historyFilename=history_file)
            cls._maya_history_file = history_file
        except Exception as e:
            _log("Warning: failed to enable Script Editor history capture: {}".format(e))
            cls._saved_script_editor = None
            cls._maya_history_file = None

        _log(
            "PRMan setUpClass: sceneDir={} | RMAN_CONFIG_OVERRIDE={} | RDIR={} | RMAN_SHADERPATH={} | PRMAN_DELEGATE_PLUGIN_PATH={} | MayaHistory={}".format(
                sceneDir,
                os.environ.get("RMAN_CONFIG_OVERRIDE", ""),
                os.environ.get("RDIR", ""),
                os.environ.get("RMAN_SHADERPATH", ""),
                os.environ.get("PRMAN_DELEGATE_PLUGIN_PATH", ""),
                getattr(cls, "_maya_history_file", None),
            )
        )

    @classmethod
    def tearDownClass(cls):
        # Dump captured output into CI log before cleanup.
        try:
            history = getattr(cls, "_maya_history_file", None)
            if history:
                cls._print_log_tail(history, "Maya Script Editor history", max_lines=400)
            else:
                _log("Maya Script Editor history: (missing) {}".format(history))
        except Exception as e:
            _log("Warning: failed to dump Maya Script Editor history: {}".format(e))

        # Restore Script Editor capture settings.
        try:
            saved = getattr(cls, "_saved_script_editor", None)
            if saved:
                cmds.scriptEditorInfo(
                    edit=True,
                    writeHistory=saved.get("writeHistory", False),
                    historyFilename=saved.get("historyFilename", ""),
                )
        except Exception as e:
            _log("Warning: failed to restore Script Editor settings: {}".format(e))

        # Restore environment variables.
        saved_env = getattr(cls, "_saved_env", None) or {}
        for k, v in saved_env.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v

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
            # Dump latest Script Editor history tail to stdout for CI visibility.
            try:
                hist = getattr(self.__class__, "_maya_history_file", None)
                self._print_log_tail(hist, "Maya Script Editor history", max_lines=200)
            except Exception:
                pass

            # Restore Maya workspace.
            try:
                cmds.workspace(saved_workspace, o=True)
            except Exception as e:
                _log("Warning: failed to restore Maya workspace: {}".format(e))

    def _print_log_tail(self, path, title, max_lines=200):
        """Print the tail of a log file to stdout so CI logs capture it."""
        if not path:
            return
        try:
            if not os.path.isfile(path):
                _log("{}: (missing) {}".format(title, path))
                return
            tail = self._tail_lines(path, max_lines)
            _log("\n===== {} (last {} lines): {} =====".format(title, len(tail), path))
            for ln in tail:
                _log(ln)
            _log("===== end {} =====\n".format(title))
        except Exception as e:
            _log("Warning: failed to print {} tail ({}): {}".format(title, path, e))

    def _tail_lines(self, path, max_lines, chunk_size=8192):
        """Read the last N lines without loading whole file."""
        try:
            with open(path, "rb") as f:
                f.seek(0, os.SEEK_END)
                end = f.tell()
                if end == 0:
                    return []
                buffer = b""
                lines = []
                while end > 0 and len(lines) <= max_lines:
                    read_size = min(chunk_size, end)
                    end -= read_size
                    f.seek(end)
                    buffer = f.read(read_size) + buffer
                    lines = buffer.splitlines()
                tail = lines[-max_lines:] if len(lines) > max_lines else lines
                return [ln.decode("utf-8", "replace") for ln in tail]
        except Exception as e:
            _log("Warning: failed to read {} tail: {}".format(path, e))
            return []

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
        """Allow slightly higher tolerance for coverage builds."""
        fail = self.IMAGE_DIFF_FAIL_THRESHOLD
        failpercent = self.IMAGE_DIFF_FAIL_PERCENT
        if (
            delegate.get("name") == "PRMan"
            and os.environ.get("MAYAHYDRA_CODE_COVERAGE")
        ):
            failpercent = max(failpercent, self.IMAGE_DIFF_FAIL_PERCENT_COVERAGE)
        return fail, failpercent

    def _delegateRunsOnPlatform(self, delegate):
        """Return True if the delegate's platform restriction allows running on the current platform."""
        required = delegate.get("platform", "all")
        if required == "all":
            return True
        return platform.system().lower() == required.lower()

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
                    with self._prmanContext():
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
    # Coverage builds can hang during Maya shutdown for this test; force exit with status.
    fixturesUtils.runTests(globals(), coverage_quit_workaround=True)
