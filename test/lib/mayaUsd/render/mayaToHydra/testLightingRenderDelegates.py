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


def _log(msg):
    """Write to original stdout so CI logs capture it (Maya may redirect sys.stdout)."""
    sys.__stdout__.write(msg + "\n")
    sys.__stdout__.flush()


def _print_log_tail_static(path, title, max_lines=200):
    """Print the tail of a log file to stdout so CI logs capture it (module-level helper)."""
    if not path:
        return
    try:
        if not os.path.isfile(path):
            _log("{}: (missing) {}".format(title, path))
            return
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = f.read().splitlines()
        tail = lines[-max_lines:] if len(lines) > max_lines else lines
        _log("\n===== {} (last {} lines): {} =====".format(title, len(tail), path))
        for ln in tail:
            _log(ln)
        _log("===== end {} =====\n".format(title))
    except Exception as e:
        _log("Warning: failed to print {} tail ({}): {}".format(title, path, e))

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

    @classmethod
    def setUpClass(cls):
        """Prime PRMan/USD search paths and enable useful logging early.

        Some components may snapshot environment variables during initialization,
        so we set key vars once at class setup time (before any renders).

        - RMAN_TEXTUREPATH: RenderMan texture search path
        - PXR_AR_DEFAULT_SEARCH_PATH: USD default resolver search path
        - TF_DEBUG: append HDPRMAN_IMAGE_ASSET_RESOLVE for hdPrman image asset resolution debug
        - Maya Script Editor history: capture delegate output and dump to CI logs
        - RenderMan config override: bump verbosity via a minimal rendermn.ini
        """
        super(TestLightingRenderDelegates, cls).setUpClass()

        scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
        sceneDir = os.path.dirname(os.path.abspath(scenePath))
        sep = os.pathsep  # ';' on Windows, ':' elsewhere

        # Save env we will modify.
        cls._saved_env = {
            "RMAN_TEXTUREPATH": os.environ.get("RMAN_TEXTUREPATH"),
            "PXR_AR_DEFAULT_SEARCH_PATH": os.environ.get("PXR_AR_DEFAULT_SEARCH_PATH"),
            "RMAN_CONFIG_OVERRIDE": os.environ.get("RMAN_CONFIG_OVERRIDE"),
            "RMAN_DUMP_DEFAULTS": os.environ.get("RMAN_DUMP_DEFAULTS"),
            "RDIR": os.environ.get("RDIR"),
            "RMAN_LOGFILE": os.environ.get("RMAN_LOGFILE"),
            "TF_DEBUG": os.environ.get("TF_DEBUG"),
        }

        # Prepend scene directory so relative ./... has a consistent anchor.
        prev_rman = cls._saved_env["RMAN_TEXTUREPATH"] or ""
        sceneDirNorm = sceneDir.replace("\\", "/")
        os.environ["RMAN_TEXTUREPATH"] = sceneDirNorm + (sep + prev_rman if prev_rman else "")

        prev_ar = cls._saved_env["PXR_AR_DEFAULT_SEARCH_PATH"] or ""
        os.environ["PXR_AR_DEFAULT_SEARCH_PATH"] = sceneDir + (sep + prev_ar if prev_ar else "")

        # Enable hdPrman image asset resolution debug output (USD/TF debug system).
        prev_tf = cls._saved_env.get("TF_DEBUG") or ""
        tf_tokens = [t.strip() for t in prev_tf.split(",") if t.strip()]
        if "HDPRMAN_IMAGE_ASSET_RESOLVE" not in tf_tokens:
            tf_tokens.append("HDPRMAN_IMAGE_ASSET_RESOLVE")
        os.environ["TF_DEBUG"] = ",".join(tf_tokens)

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
        # RMAN_LOGFILE: redirect PRMan log to file so we can dump it to CI (if supported).
        prman_log = os.path.join(log_root, "prman.log")
        cls._prman_log_file = prman_log
        cls._prman_log_root = log_root
        os.environ["RMAN_LOGFILE"] = prman_log

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
            "PRMan setUpClass: sceneDir={} | RMAN_TEXTUREPATH={} | PXR_AR_DEFAULT_SEARCH_PATH={} | RMAN_CONFIG_OVERRIDE={} | RDIR={} | RMAN_LOGFILE={} | TF_DEBUG={} | MayaHistory={}".format(
                sceneDir,
                os.environ.get("RMAN_TEXTUREPATH", ""),
                os.environ.get("PXR_AR_DEFAULT_SEARCH_PATH", ""),
                os.environ.get("RMAN_CONFIG_OVERRIDE", ""),
                os.environ.get("RDIR", ""),
                os.environ.get("RMAN_LOGFILE", ""),
                os.environ.get("TF_DEBUG", ""),
                getattr(cls, "_maya_history_file", None),
            )
        )

    @classmethod
    def tearDownClass(cls):
        # Dump captured output into CI log before cleanup.
        try:
            history = getattr(cls, "_maya_history_file", None)
            if history and os.path.isfile(history):
                with open(history, "r", encoding="utf-8", errors="replace") as f:
                    lines = f.read().splitlines()
                tail = lines[-400:] if len(lines) > 400 else lines
                _log("\n===== Maya Script Editor history (last {} lines): {} =====".format(len(tail), history))
                for ln in tail:
                    _log(ln)
                _log("===== end Maya Script Editor history =====\n")
            else:
                _log("Maya Script Editor history: (missing) {}".format(history))
        except Exception as e:
            _log("Warning: failed to dump Maya Script Editor history: {}".format(e))

        # Dump PRMan log file if it exists (RMAN_LOGFILE redirects PRMan output to file).
        try:
            prman_log = getattr(cls, "_prman_log_file", None)
            log_root = getattr(cls, "_prman_log_root", None)
            if prman_log and os.path.isfile(prman_log):
                _print_log_tail_static(prman_log, "PRMan log", max_lines=500)
            else:
                _log("PRMan log: (not found) {} ".format(prman_log or "(RMAN_LOGFILE not set)"))
            # Also dump any other .log files in the PRMan log dir (PRMan may use different names).
            if log_root and os.path.isdir(log_root):
                for name in sorted(os.listdir(log_root)):
                    if name.endswith(".log") and os.path.join(log_root, name) != prman_log:
                        p = os.path.join(log_root, name)
                        _print_log_tail_static(p, "PRMan log ({})".format(name), max_lines=300)
        except Exception as e:
            _log("Warning: failed to dump PRMan log: {}".format(e))

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
    def _prmanTexturePath(self):
        """Per-render context for PRMan/hdPrman texture resolution and diagnostics.

        Environment (RMAN_TEXTUREPATH / PXR_AR_DEFAULT_SEARCH_PATH / TF_DEBUG) is primed
        in setUpClass() to ensure it is applied before renderer initialization.

        Here we ensure:
          - cwd is the scene directory (so ./foo.png works)
          - Maya workspace (project) is the scene directory
          - Maya file-node paths are temporarily rewritten to absolute paths
          - we dump the Script Editor history tail to stdout for CI visibility
        """
        saved_cwd = os.getcwd()
        saved_workspace = cmds.workspace(q=True, rd=True)

        try:
            scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
            sceneDir = os.path.dirname(os.path.abspath(scenePath))

            os.chdir(sceneDir)
            cmds.workspace(sceneDir, o=True)

            saved_file_nodes = self._absolutizeFileTextures(sceneDir)

            diffusePath = os.path.join(sceneDir, "diffuse.png")
            uvCheckerPath = os.path.join(sceneDir, "UVChecker.png")
            _log(
                "PRMan context: cwd={} | workspace={} | diffuse.png exists={} | UVChecker.png exists={} | RMAN_TEXTUREPATH={} | PXR_AR_DEFAULT_SEARCH_PATH={} | TF_DEBUG={}".format(
                    os.getcwd(),
                    cmds.workspace(q=True, rd=True),
                    os.path.isfile(diffusePath),
                    os.path.isfile(uvCheckerPath),
                    os.environ.get("RMAN_TEXTUREPATH", ""),
                    os.environ.get("PXR_AR_DEFAULT_SEARCH_PATH", ""),
                    os.environ.get("TF_DEBUG", ""),
                )
            )

            yield
        finally:
            # Restore any file-node edits first.
            try:
                self._restoreFileTextures(saved_file_nodes)
            except Exception:
                pass

            # Dump latest Script Editor history tail to stdout for CI visibility.
            try:
                hist = getattr(self.__class__, "_maya_history_file", None)
                self._print_log_tail(hist, "Maya Script Editor history", max_lines=200)
            except Exception:
                pass

            # Restore Maya workspace and cwd.
            try:
                cmds.workspace(saved_workspace, o=True)
            except Exception as e:
                _log("Warning: failed to restore Maya workspace: {}".format(e))
            try:
                os.chdir(saved_cwd)
            except Exception as e:
                _log("Warning: failed to restore cwd: {}".format(e))

    def _absolutizeFileTextures(self, sceneDir):
        """Rewrite Maya file-node texture paths to absolute paths for reliable CI runs.

        Maya ASCII scenes often store fileTextureName as a relative string (e.g. ./diffuse.png).
        Depending on the Maya->Hydra adapter, these strings may be passed through without
        being anchored to the Maya project/workspace or USD resolver context.

        By converting to absolute paths in the scene graph, we guarantee that Hydra/hdPrman
        receives a resolvable path regardless of the process working directory.

        Returns:
            dict[str,str]: file-node name -> original fileTextureName
        """
        saved = {}
        file_nodes = cmds.ls(type="file") or []
        for n in file_nodes:
            try:
                p = cmds.getAttr(n + ".fileTextureName") or ""
            except Exception:
                continue
            if not p or os.path.isabs(p):
                continue
            abs_p = os.path.normpath(os.path.join(sceneDir, p))
            if p.startswith("./") or os.path.isfile(abs_p):
                saved[n] = p
                cmds.setAttr(n + ".fileTextureName", abs_p, type="string")

        if saved:
            _log("PRMan context: rewrote {} file-node texture path(s) to absolute.".format(len(saved)))
        return saved

    def _restoreFileTextures(self, saved):
        """Restore original fileTextureName values saved by _absolutizeFileTextures."""
        if not saved:
            return
        for n, p in saved.items():
            try:
                cmds.setAttr(n + ".fileTextureName", p, type="string")
            except Exception:
                pass

    def _print_log_tail(self, path, title, max_lines=200):
        """Print the tail of a log file to stdout so CI logs capture it."""
        if not path:
            return
        try:
            if not os.path.isfile(path):
                _log("{}: (missing) {}".format(title, path))
                return
            with open(path, "r", encoding="utf-8", errors="replace") as f:
                lines = f.read().splitlines()
            tail = lines[-max_lines:] if len(lines) > max_lines else lines
            _log("\n===== {} (last {} lines): {} =====".format(title, len(tail), path))
            for ln in tail:
                _log(ln)
            _log("===== end {} =====\n".format(title))
        except Exception as e:
            _log("Warning: failed to print {} tail ({}): {}".format(title, path, e))

    def _setRenderer(self, delegate):
        """Switch the viewport to the given Hydra renderer."""
        panel = cmds.playblast(activeEditor=1)
        cmds.modelEditor(panel, edit=True, rendererOverrideName=delegate["override"])
        cmds.refresh(force=True)

        # Validate via mayaHydra (modelEditor query can be unreliable when MAYAUSD_DISABLE_VP2_RENDER_DELEGATE=1)
        activeRenderers = cmds.mayaHydra(listActiveRenderers=True)
        _log("Active renderers: {}".format(activeRenderers))
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
            return
        rendererPlugin = delegate["plugin"]
        start = time.time()
        while (time.time() - start) < timeoutSeconds:
            cmds.refresh(force=True)
            if cmds.mayaHydraTesting(converged=True, rendererName=rendererPlugin):
                cmds.refresh(force=True)
                return
            time.sleep(0.5)
        # Timeout reached; some renderers (e.g. PRMan) never report convergence in interactive mode.
        cmds.refresh(force=True)

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
