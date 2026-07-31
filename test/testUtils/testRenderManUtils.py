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

"""Standalone unit tests for the pure-Python logic in renderManUtils.

These tests deliberately avoid Maya: renderManUtils only touches maya.cmds
through _isRendererAvailable(), which is patched out here.  This lets the
platform/CI gating logic (the part most easily broken by a refactor) be
verified with a fast mayapy/python run instead of a full interactive Maya
session.
"""

import os
import sys
import unittest
from unittest import mock

import renderManUtils


# Environment variables the gating/debug-logging logic reads. They are cleared
# before each test so the baseline is deterministic even on a CI machine (which
# sets JENKINS_URL / BUILD_ID for real) or a developer machine with
# MAYAHYDRA_TEST_DEBUG set. In particular, leaving MAYAHYDRA_TEST_DEBUG set
# would make _testDebugEnabled() true, causing the extra time.time() call in
# waitForInteractiveConvergence()'s debug-logging branch to consume an
# unplanned value from a test's mocked time.time() side_effect list and raise
# StopIteration.
_ENV_KEYS = (
    "MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS",
    "JENKINS_URL",
    "BUILD_ID",
    "MAYAHYDRA_TEST_DEBUG",
)


class RenderManUtilsTestCase(unittest.TestCase):
    def setUp(self):
        self._savedEnv = {k: os.environ.get(k) for k in _ENV_KEYS}
        for k in _ENV_KEYS:
            os.environ.pop(k, None)

    def tearDown(self):
        for k, v in self._savedEnv.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v

    # -- _getAllowedPlatforms ------------------------------------------------

    def test_getAllowedPlatforms_empty_when_unset(self):
        self.assertEqual(renderManUtils._getAllowedPlatforms(), [])

    def test_getAllowedPlatforms_comma_separated_lowercased(self):
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "Windows, Linux"
        self.assertEqual(
            renderManUtils._getAllowedPlatforms(), ["windows", "linux"])

    def test_getAllowedPlatforms_semicolons_and_empties_dropped(self):
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "windows; ,linux ;"
        self.assertEqual(
            renderManUtils._getAllowedPlatforms(), ["windows", "linux"])

    # -- _isCiBuild ----------------------------------------------------------

    def test_isCiBuild_false_by_default(self):
        self.assertFalse(renderManUtils._isCiBuild())

    def test_isCiBuild_true_with_jenkins_url(self):
        os.environ["JENKINS_URL"] = "http://ci.example/"
        self.assertTrue(renderManUtils._isCiBuild())

    def test_isCiBuild_true_with_build_id(self):
        os.environ["BUILD_ID"] = "42"
        self.assertTrue(renderManUtils._isCiBuild())

    # -- _failOnCi -----------------------------------------------------------

    def test_failOnCi_raises_without_fail_fn(self):
        with self.assertRaises(RuntimeError):
            renderManUtils._failOnCi(None, "boom")

    def test_failOnCi_calls_fail_fn_then_raises(self):
        seen = []
        with self.assertRaises(RuntimeError):
            renderManUtils._failOnCi(seen.append, "boom")
        self.assertEqual(seen, ["boom"])

    # -- shouldRunDelegate ---------------------------------------------------

    def test_shouldRunDelegate_skips_when_not_allowed_off_ci(self):
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"):
            self.assertFalse(renderManUtils.shouldRunDelegate())

    def test_shouldRunDelegate_skips_unsupported_platform(self):
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "linux"
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"):
            self.assertFalse(renderManUtils.shouldRunDelegate())

    def test_shouldRunDelegate_true_when_available(self):
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "windows"
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"), \
             mock.patch.object(renderManUtils, "_isRendererAvailable",
                               return_value=(True, [renderManUtils.HD_PRMAN])):
            self.assertTrue(renderManUtils.shouldRunDelegate())

    def test_shouldRunDelegate_skips_when_renderer_unavailable_off_ci(self):
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "windows"
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"), \
             mock.patch.object(renderManUtils, "_isRendererAvailable",
                               return_value=(False, [])):
            self.assertFalse(renderManUtils.shouldRunDelegate())

    def test_shouldRunDelegate_raises_on_ci_when_platforms_unset_without_fail_fn(self):
        # Regression: the "required on CI" invariant must hold even when the
        # caller omits fail_fn (previously this silently returned False).
        os.environ["JENKINS_URL"] = "http://ci.example/"
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"):
            with self.assertRaises(RuntimeError):
                renderManUtils.shouldRunDelegate(fail_fn=None)

    def test_shouldRunDelegate_raises_on_ci_when_renderer_unavailable_without_fail_fn(self):
        os.environ["JENKINS_URL"] = "http://ci.example/"
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "windows"
        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"), \
             mock.patch.object(renderManUtils, "_isRendererAvailable",
                               return_value=(False, [])):
            with self.assertRaises(RuntimeError):
                renderManUtils.shouldRunDelegate(fail_fn=None)

    def test_shouldRunDelegate_prefers_fail_fn_on_ci(self):
        os.environ["JENKINS_URL"] = "http://ci.example/"
        os.environ["MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS"] = "windows"
        calls = []

        def fakeFail(msg):
            calls.append(msg)
            raise AssertionError(msg)

        with mock.patch.object(renderManUtils.platform, "system",
                               return_value="Windows"), \
             mock.patch.object(renderManUtils, "_isRendererAvailable",
                               return_value=(False, [])):
            with self.assertRaises(AssertionError):
                renderManUtils.shouldRunDelegate(fail_fn=fakeFail)
        self.assertEqual(len(calls), 1)

    # -- setUp / tearDown ----------------------------------------------------

    def test_setUp_configures_non_adaptive_sampling_env(self):
        saved = renderManUtils.setUp()
        try:
            self.assertEqual(os.environ["HD_PRMAN_DISABLE_ADAPTIVE_SAMPLING"], "1")
            self.assertEqual(
                os.environ["HD_PRMAN_MAX_SAMPLES"],
                str(renderManUtils.PRMAN_TEST_MAX_SAMPLES),
            )
        finally:
            renderManUtils.tearDown(saved)

    def test_tearDown_restores_sampling_env(self):
        os.environ["HD_PRMAN_DISABLE_ADAPTIVE_SAMPLING"] = "0"
        os.environ["HD_PRMAN_MAX_SAMPLES"] = "64"
        saved = renderManUtils.setUp()
        renderManUtils.tearDown(saved)
        self.assertEqual(os.environ["HD_PRMAN_DISABLE_ADAPTIVE_SAMPLING"], "0")
        self.assertEqual(os.environ["HD_PRMAN_MAX_SAMPLES"], "64")

    def test_estimateRenderSettleSeconds_matches_max_settle_constant(self):
        self.assertEqual(
            renderManUtils.estimateRenderSettleSeconds(),
            renderManUtils.PRMAN_TEST_MAX_SETTLE_SECONDS,
        )

    # -- waitForInteractiveConvergence ---------------------------------------

    def test_waitForInteractiveConvergence_returns_true_when_converged(self):
        mock_cmds = mock.MagicMock()
        mock_cmds.mayaHydraTesting.return_value = True
        mock_maya = mock.MagicMock()
        mock_maya.cmds = mock_cmds
        with mock.patch.dict(sys.modules, {"maya": mock_maya, "maya.cmds": mock_cmds}), \
             mock.patch.object(renderManUtils.time, "time", side_effect=[0.0, 0.1]), \
             mock.patch.object(renderManUtils.time, "sleep") as mock_sleep:
            self.assertTrue(renderManUtils.waitForInteractiveConvergence())
        mock_cmds.mayaHydraTesting.assert_called_once_with(
            converged=True, rendererName=renderManUtils.HD_PRMAN
        )
        mock_sleep.assert_not_called()

    def test_waitForInteractiveConvergence_refreshes_before_checking_convergence(self):
        # Regression: mayaHydraTesting(converged=True) only reflects the last
        # Execute() call. Without an explicit refresh each poll iteration, the
        # flag can never change and this would always spin for the full
        # timeout instead of detecting convergence early.
        mock_cmds = mock.MagicMock()
        mock_cmds.mayaHydraTesting.return_value = True
        mock_maya = mock.MagicMock()
        mock_maya.cmds = mock_cmds
        with mock.patch.dict(sys.modules, {"maya": mock_maya, "maya.cmds": mock_cmds}), \
             mock.patch.object(renderManUtils.time, "time", side_effect=[0.0, 0.1]), \
             mock.patch.object(renderManUtils.time, "sleep") as mock_sleep:
            self.assertTrue(renderManUtils.waitForInteractiveConvergence())
        mock_cmds.refresh.assert_called_once_with(force=True)
        # The refresh must happen before the convergence check is trusted.
        self.assertLess(
            mock_cmds.method_calls.index(mock.call.refresh(force=True)),
            mock_cmds.method_calls.index(
                mock.call.mayaHydraTesting(
                    converged=True, rendererName=renderManUtils.HD_PRMAN
                )
            ),
        )
        mock_sleep.assert_not_called()

    def test_waitForInteractiveConvergence_times_out_without_convergence(self):
        mock_cmds = mock.MagicMock()
        mock_cmds.mayaHydraTesting.return_value = False
        mock_maya = mock.MagicMock()
        mock_maya.cmds = mock_cmds
        # start, then one time.time() timeout-check per loop iteration, until timeout
        times = [0.0]
        t = 0.0
        while t <= 5.0:
            t += 0.25
            times.append(t)
        with mock.patch.dict(sys.modules, {"maya": mock_maya, "maya.cmds": mock_cmds}), \
             mock.patch.object(renderManUtils.time, "time", side_effect=times), \
             mock.patch.object(renderManUtils.time, "sleep") as mock_sleep:
            self.assertFalse(
                renderManUtils.waitForInteractiveConvergence(
                    timeout_seconds=5.0, poll_interval_seconds=0.25
                )
            )
        self.assertGreater(mock_cmds.mayaHydraTesting.call_count, 1)
        # Regression: each poll iteration must refresh, or convergence can
        # never be observed and the loop degenerates into a fixed sleep.
        self.assertEqual(mock_cmds.refresh.call_count, mock_cmds.mayaHydraTesting.call_count)
        self.assertGreater(mock_sleep.call_count, 0)


if __name__ == "__main__":
    unittest.main(verbosity=2)
