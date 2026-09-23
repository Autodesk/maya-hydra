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

"""Standalone unit tests for the Maya-free logic in imageDiffUtils.

These tests deliberately avoid Maya and never launch a real idiff/oiiotool
process: imageDiff() and generateDiffImage() are patched out so the tests
run fast under mayapy (or plain Python) via unittest, exercising only the
pass/fail policy, warn/fail mirroring, and failure-report formatting.
"""

import os
import unittest
from unittest import mock

import imageDiffUtils


# Environment variables the artifact-URL logic reads. Cleared before each
# test so the baseline is deterministic even on CI (which sets these for
# real Jenkins builds).
_ENV_KEYS = ("JENKINS_ARTIFACT_BASE", "JENKINS_ARTIFACT_WORKSPACE", "WORKSPACE")


class _FakeCompletedProcess(object):
    """Minimal stand-in for subprocess.CompletedProcess."""

    def __init__(self, returncode, stdout="", stderr=""):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr


class ReportImageComparisonFailureTestCase(unittest.TestCase):
    def setUp(self):
        self._savedEnv = {k: os.environ.get(k) for k in _ENV_KEYS}
        for k in _ENV_KEYS:
            os.environ.pop(k, None)
        # Never actually shell out to generate a diff image in these tests.
        self._generateDiffImagePatch = mock.patch.object(
            imageDiffUtils, "generateDiffImage", return_value=None)
        self._generateDiffImagePatch.start()

    def tearDown(self):
        self._generateDiffImagePatch.stop()
        for k, v in self._savedEnv.items():
            if v is None:
                os.environ.pop(k, None)
            else:
                os.environ[k] = v

    def test_plain_paths_when_artifact_base_unset(self):
        baseline = os.path.join("work", "baseline.png")
        actual = os.path.join("work", "actual.png")
        msg = imageDiffUtils.reportImageComparisonFailure(baseline, actual, "idiff stdout")
        expectedBaseline = os.path.abspath(baseline).replace('\\', '/')
        expectedActual = os.path.abspath(actual).replace('\\', '/')
        self.assertIn("idiff stdout", msg)
        self.assertIn("Image comparison failed.", msg)
        self.assertIn("Baseline: " + expectedBaseline, msg)
        self.assertIn("Actual:   " + expectedActual, msg)
        # No artifact base configured -> no URLs, no Browse line.
        self.assertNotIn("Browse:", msg)

    def test_idiff_stderr_appended_when_non_empty(self):
        msg = imageDiffUtils.reportImageComparisonFailure(
            os.path.join("work", "baseline.png"), os.path.join("work", "actual.png"),
            "idiff stdout", idiffStderr="idiff stderr")
        self.assertIn("idiff stderr", msg)

    def test_idiff_stderr_omitted_when_empty(self):
        msg = imageDiffUtils.reportImageComparisonFailure(
            os.path.join("work", "baseline.png"), os.path.join("work", "actual.png"),
            "idiff stdout", idiffStderr="")
        self.assertNotIn("idiff stderr", msg)

    def test_artifact_urls_built_when_base_and_workspace_set(self):
        os.environ["JENKINS_ARTIFACT_BASE"] = "https://jenkins.example/artifacts"
        os.environ["JENKINS_ARTIFACT_WORKSPACE"] = os.path.join("work")
        baseline = os.path.join("work", "baseline.png")
        actual = os.path.join("work", "actual.png")
        msg = imageDiffUtils.reportImageComparisonFailure(baseline, actual, "idiff stdout")
        self.assertIn(
            "Baseline: https://jenkins.example/artifacts/baseline.png", msg)
        self.assertIn(
            "Actual:   https://jenkins.example/artifacts/actual.png", msg)
        self.assertIn("Browse:   https://jenkins.example/artifacts/", msg)

    def test_workspace_falls_back_to_WORKSPACE_env(self):
        os.environ["JENKINS_ARTIFACT_BASE"] = "https://jenkins.example/artifacts"
        os.environ["WORKSPACE"] = os.path.join("work")
        baseline = os.path.join("work", "baseline.png")
        actual = os.path.join("work", "actual.png")
        msg = imageDiffUtils.reportImageComparisonFailure(baseline, actual, "idiff stdout")
        self.assertIn(
            "Baseline: https://jenkins.example/artifacts/baseline.png", msg)

    def test_falls_back_to_common_ancestor_when_workspace_relpath_fails(self):
        os.environ["JENKINS_ARTIFACT_BASE"] = "https://jenkins.example/artifacts"
        # Workspace unrelated to the image paths -- relpath against it
        # should fail validation (the '..'-prefix guard), so the
        # commonpath-of-both-images fallback (their shared parent directory)
        # should kick in instead.
        os.environ["JENKINS_ARTIFACT_WORKSPACE"] = os.path.join("unrelated", "elsewhere")
        baseline = os.path.join("shared", "renders", "baseline.png")
        actual = os.path.join("shared", "renders", "actual.png")
        msg = imageDiffUtils.reportImageComparisonFailure(baseline, actual, "idiff stdout")
        self.assertIn(
            "Baseline: https://jenkins.example/artifacts/baseline.png", msg)
        self.assertIn(
            "Actual:   https://jenkins.example/artifacts/actual.png", msg)

    def test_diff_url_included_when_generateDiffImage_succeeds(self):
        os.environ["JENKINS_ARTIFACT_BASE"] = "https://jenkins.example/artifacts"
        os.environ["JENKINS_ARTIFACT_WORKSPACE"] = os.path.join("work")
        diff_path = os.path.join("work", "actual_diff.png")
        self._generateDiffImagePatch.stop()
        with mock.patch.object(imageDiffUtils, "generateDiffImage", return_value=diff_path):
            msg = imageDiffUtils.reportImageComparisonFailure(
                os.path.join("work", "baseline.png"),
                os.path.join("work", "actual.png"),
                "idiff stdout")
        self._generateDiffImagePatch.start()
        self.assertIn(
            "Diff:     https://jenkins.example/artifacts/actual_diff.png", msg)

    def test_diff_plain_path_included_without_artifact_base(self):
        diff_path = os.path.join("work", "actual_diff.png")
        self._generateDiffImagePatch.stop()
        with mock.patch.object(imageDiffUtils, "generateDiffImage", return_value=diff_path):
            msg = imageDiffUtils.reportImageComparisonFailure(
                os.path.join("work", "baseline.png"),
                os.path.join("work", "actual.png"),
                "idiff stdout")
        self._generateDiffImagePatch.start()
        self.assertIn("Diff:", msg)
        self.assertIn("actual_diff.png", msg)


class CompareImagePairTestCase(unittest.TestCase):
    def test_default_passReturnCodes_rc0_passes(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(0)) as mockDiff:
            result = imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1)
        self.assertTrue(result.passed)
        self.assertIsNone(result.message)
        self.assertEqual(result.returncode, 0)
        mockDiff.assert_called_once()

    def test_default_passReturnCodes_rc1_fails(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(1, stdout="warn")), \
             mock.patch.object(imageDiffUtils, "reportImageComparisonFailure",
                                return_value="failure report") as mockReport:
            result = imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1)
        self.assertFalse(result.passed)
        self.assertEqual(result.message, "failure report")
        self.assertEqual(result.returncode, 1)
        mockReport.assert_called_once()

    def test_default_passReturnCodes_rc2_fails(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(2, stdout="error")), \
             mock.patch.object(imageDiffUtils, "reportImageComparisonFailure",
                                return_value="failure report"):
            result = imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1)
        self.assertFalse(result.passed)
        self.assertEqual(result.returncode, 2)

    def test_lenient_passReturnCodes_rc1_passes(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(1)) as mockDiff:
            result = imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1,
                passReturnCodes=(0, 1))
        self.assertTrue(result.passed)
        self.assertEqual(result.returncode, 1)
        mockDiff.assert_called_once()

    def test_warn_mirrors_fail_when_omitted(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(0)) as mockDiff:
            imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1)
        _, kwargs = mockDiff.call_args
        self.assertEqual(kwargs["warn"], 0.004)
        self.assertEqual(kwargs["warnpercent"], 1)

    def test_explicit_warn_not_overridden(self):
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(0)) as mockDiff:
            imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1,
                warn=0.01, warnpercent=5)
        _, kwargs = mockDiff.call_args
        self.assertEqual(kwargs["warn"], 0.01)
        self.assertEqual(kwargs["warnpercent"], 5)

    def test_no_mirroring_when_fail_is_none(self):
        # assertImagesEqual() passes fail=None, failpercent=None and relies on
        # idiff's own defaults -- warn/warnpercent must stay None too.
        with mock.patch.object(imageDiffUtils, "imageDiff",
                                return_value=_FakeCompletedProcess(0)) as mockDiff:
            imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=None, failpercent=None)
        _, kwargs = mockDiff.call_args
        self.assertIsNone(kwargs["warn"])
        self.assertIsNone(kwargs["warnpercent"])

    def test_exec_failure_returns_not_passed(self):
        with mock.patch.object(imageDiffUtils, "imageDiff", return_value=None):
            result = imageDiffUtils.compareImagePair(
                "baseline.png", "actual.png", fail=0.004, failpercent=1)
        self.assertFalse(result.passed)
        self.assertIsNone(result.returncode)
        self.assertIn("Failed to execute imageDiff", result.message)


if __name__ == "__main__":
    unittest.main(verbosity=2)
