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
from contextlib import contextmanager

import maya.cmds as cmds

import fixturesUtils
import mayaUtils
import mtohUtils
import renderManUtils
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
# convergenceTimeout: seconds to poll mayaHydraTesting(converged=True) before playblast
# snapshot (0 = skip). Only read via _waitForConvergence(), which _runLightSnapshots()
# calls for playblast-captured delegates (i.e. not PRMan -- see use_hydra_writer in
# _runLightSnapshots). PRMan does not use this field at all: it is always captured via
# hydraSnapshot/useHydraWriter and settles via hydraSettleFn (see
# _settlePrmanBeforeSnapshot / renderManUtils.waitForInteractiveConvergence), so it has
# no convergenceTimeout entry below.
# platform: "all" (default) or "windows" to restrict image comparison to that platform.
# failThreshold: per-pixel difference threshold passed to idiff -fail (default 0.1).
# failPercent: percentage of failing pixels passed to idiff -failpercent (default 7.0).
RENDER_DELEGATES = [
    {
        "name": "Storm",
        "plugin": mtohUtils.HD_STORM,
        "override": mtohUtils.HD_STORM_OVERRIDE,
        "mayaPlugin": None,
        "convergenceTimeout": 0,  # Storm converges immediately
        "failThreshold": 0.1,
        "failPercent": 7.0,
    },
    # Disabling Arnold render delegate test, logged as HYDRA-2122 and HYDRA-2123
    # {
    #     "name": "Arnold",
    #     "plugin": "HdArnoldRendererPlugin",
    #     "override": "mayaHydraRenderOverride_HdArnoldRendererPlugin",
    #     "mayaPlugin": "mtoa",
    #     "convergenceTimeout": 30,
    #     "failThreshold": 0.1,
    #     "failPercent": 7.0,
    # },
    {
        "name": "PRMan",
        "plugin": renderManUtils.HD_PRMAN,
        "override": renderManUtils.HD_PRMAN_OVERRIDE,
        "mayaPlugin": None,
        # No convergenceTimeout: PRMan is always captured via hydraSnapshot and
        # settles via hydraSettleFn (see _settlePrmanBeforeSnapshot), so
        # _waitForConvergence() is never called for it.
        # PRMan lighting in the interactive viewport is not deterministic: progressive
        # sampling, tone-mapping, and the light transport model all differ from Storm,
        # so a strict pixel-accurate comparison would always fail.  These thresholds
        # are intentionally loose: the test is a smoke check that verifies a non-blank
        # image is produced with the expected geometry and textures visible, not a
        # photometric comparison of the lighting.
        "failThreshold": 0.2,
        "failPercent": 10.0,
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
        # Env vars must be set before HdPrman is first loaded by Maya/USD.
        if any(d["plugin"] == renderManUtils.HD_PRMAN for d in RENDER_DELEGATES):
            cls._prman_saved_env = renderManUtils.setUp()
            if "mayaHydraCppTests" not in cls._requiredPlugins:
                cls._requiredPlugins.append("mayaHydraCppTests")

        super(TestLightingRenderDelegates, cls).setUpClass()

    @classmethod
    def tearDownClass(cls):
        renderManUtils.tearDown(getattr(cls, "_prman_saved_env", None))

        # MayaHydraBaseTestCase.tearDownClass runs cmds.file(new=True, force=True) and
        # unloads plugins; that can hang indefinitely after a failed PRMan test. Coverage
        # runs use fixturesUtils.runTests() with os._exit (via MAYAHYDRA_CODE_COVERAGE),
        # so skipping base teardown is safe for this single-class module.
        if os.environ.get("MAYAHYDRA_CODE_COVERAGE") and any(
            d["plugin"] == renderManUtils.HD_PRMAN for d in RENDER_DELEGATES
        ):
            sys.__stdout__.write(
                "LightingRenderDelegates: skipping MayaHydraBaseTestCase.tearDownClass "
                "(file new / unloadPlugin can hang after PRMan failure)\n"
            )
            sys.__stdout__.flush()
            return

        super(TestLightingRenderDelegates, cls).tearDownClass()

    @contextmanager
    def _sceneWorkspaceContext(self):
        """Temporarily set the Maya workspace root to the test scene directory.

        Some renderers (e.g. PRMan) resolve relative output paths against the
        workspace root, so this ensures render outputs land in the expected
        location without changing the process working directory.
        """
        saved_workspace = cmds.workspace(q=True, rd=True)

        try:
            scenePath = getTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
            sceneDir = os.path.dirname(os.path.abspath(scenePath))

            cmds.workspace(sceneDir, o=True)

            yield
        finally:
            try:
                cmds.workspace(saved_workspace, o=True)
            except Exception as e:
                self.trace("Warning: failed to restore Maya workspace: {}\n".format(e))

    def _setRenderer(self, delegate):
        """Switch the viewport to the given Hydra renderer."""
        is_prman = delegate.get("plugin") == renderManUtils.HD_PRMAN
        if is_prman:
            renderManUtils.logDiagnostics("before renderer switch")
        panel = mayaUtils.activeModelPanel()
        cmds.modelEditor(panel, edit=True, rendererOverrideName=delegate["override"])
        cmds.refresh(force=True)

        activeRenderers = cmds.mayaHydra(listActiveRenderers=True)
        if is_prman and delegate["plugin"] not in activeRenderers:
            renderManUtils.logDiagnostics("after renderer switch - not active")
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
        """Wait for progressive renderers to converge before a playblast snapshot.

        Uses mayaHydraTesting(converged=True). Skips when convergenceTimeout is 0
        or absent (e.g. Storm, which converges immediately). Not called at all for
        PRMan, which is captured via hydraSnapshot/hydraSettleFn instead -- see
        use_hydra_writer in _runLightSnapshots and _settlePrmanBeforeSnapshot.
        If the renderer does not report convergence within the timeout, proceeds anyway.
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
        cmds.refresh(force=True)

    # Settle PRMan after setImageSize, then capture via captureRefresh().
    #
    # Pass as assertSnapshotClose(hydraSettleFn=...) for PRMan. Polls
    # mayaHydraTesting(converged=True) at capture resolution, refreshing on
    # every iteration so convergence can actually be detected (see
    # renderManUtils.waitForInteractiveConvergence), then captureRefresh()
    # writes the file.
    #
    # Usage:
    #   self.assertSnapshotClose(baseline, fail, failpercent,
    #       useHydraWriter=True, hydraSettleFn=self._settlePrmanBeforeSnapshot)
    def _settlePrmanBeforeSnapshot(self, captureRefresh):
        renderManUtils.waitForInteractiveConvergence()
        captureRefresh()

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
        use_hydra_writer = delegate.get("plugin") == renderManUtils.HD_PRMAN
        for lightName in LIGHTS:
            intensity = self._getIntensityForLight(lightName, delegate)
            self._setLightIntensities(lightName, intensity)
            cmds.refresh(force=True)
            if not use_hydra_writer:
                self._waitForConvergence(delegate)
            baselineName = "{}_{}.png".format(delegate["name"], lightName)
            self.assertSnapshotClose(
                baselineName,
                fail,
                failpercent,
                useHydraWriter=use_hydra_writer,
                # setImageSize (called inside hydraSnapshot, before the capture
                # refresh) resizes the render buffers, which restarts PRMan's
                # progressive render at the new resolution. Settling must
                # happen after that resize, not before it, or the capture
                # sees zero samples. See _settlePrmanBeforeSnapshot.
                hydraSettleFn=self._settlePrmanBeforeSnapshot if use_hydra_writer else None,
            )

    def _getImageDiffThresholds(self, delegate):
        """Read per-delegate thresholds, with a coverage-build bump on failpercent."""
        fail = delegate.get("failThreshold", self.IMAGE_DIFF_FAIL_THRESHOLD)
        failpercent = delegate.get("failPercent", self.IMAGE_DIFF_FAIL_PERCENT)
        if os.environ.get("MAYAHYDRA_CODE_COVERAGE"):
            failpercent = max(failpercent, self.IMAGE_DIFF_FAIL_PERCENT_COVERAGE)
        return fail, failpercent

    def _delegateRunsOnPlatform(self, delegate):
        """Return True if the delegate's platform restriction allows running on the current platform."""
        required = delegate.get("platform", "all")
        if required == "all":
            return True
        return platform.system().lower() == required.lower()

    def _setupSnapshotViewport(self):
        """Pin the snapshot camera and display flags used when baselines were captured.

        The test scene stores camera1 on the Persp panel in its embedded UI config,
        but that config is not always restored in batch/standalone Maya (depends on
        Maya version and $gUseScenePanelConfig). Without an explicit lookThru, the
        active editor can keep a default persp view and snapshot comparisons fail
        with a large framing mismatch rather than lighting noise.
        """
        panel = cmds.playblast(activeEditor=True)
        cmds.lookThru(panel, "camera1")
        cmds.modelEditor(
            panel,
            edit=True,
            displayLights="all",
            displayTextures=True,
            shadows=True,
            grid=False,
        )
        cmds.refresh(force=True)

    def loadScene(self):
        """Load the test scene and set the base renderer state."""
        mayaUtils.openTestScene("testLightingRenderDelegates", "testLightingRenderDelegates.ma")
        self.setHdStormRenderer()
        self._setupSnapshotViewport()

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
                if delegate["plugin"] == renderManUtils.HD_PRMAN:
                    if not renderManUtils.shouldRunDelegate(fail_fn=self.fail):
                        continue
                    with self._sceneWorkspaceContext():
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
                if not (
                    os.environ.get("MAYAHYDRA_CODE_COVERAGE")
                    and sys.exc_info()[0] is not None
                ):
                    self._setRenderer(RENDER_DELEGATES[0])
            except Exception:
                pass


if __name__ == "__main__":
    # Coverage builds can hang during Maya shutdown for this test; force exit with status.
    fixturesUtils.runTests(globals())
