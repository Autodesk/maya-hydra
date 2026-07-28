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

"""Utilities for tests that exercise the RenderMan (PRMan) Hydra delegate."""

import os
import platform
import sys
import tempfile


HD_PRMAN = "HdPrmanLoaderRendererPlugin"
HD_PRMAN_OVERRIDE = "mayaHydraRenderOverride_" + HD_PRMAN

_PRMAN_ENV_KEYS = ("RMAN_CONFIG_OVERRIDE", "RDIR")


def _testDebugEnabled():
    """Return True when verbose PRMan test diagnostics are requested."""
    return bool(os.environ.get("MAYAHYDRA_TEST_DEBUG"))


def setUp():
    """Configure the PRMan environment for a test class.

    Redirects RMAN_CONFIG_OVERRIDE / RDIR to a temporary directory so test
    runs are isolated from the user's RenderMan config.

    Returns:
        A dict mapping each modified env-var name to its original value
        (or None when it was unset).  Pass this dict to tearDown() to
        restore the environment after the test class completes.
    """
    saved_env = {k: os.environ.get(k) for k in _PRMAN_ENV_KEYS}

    log_root = os.path.join(tempfile.gettempdir(), "mayaHydra_prman_logs")
    os.makedirs(log_root, exist_ok=True)

    os.environ["RMAN_CONFIG_OVERRIDE"] = log_root
    os.environ["RDIR"] = log_root

    if _testDebugEnabled():
        sys.__stdout__.write(
            "PRMan setUp: RMAN_CONFIG_OVERRIDE={} | RDIR={}"
            " | RMAN_SHADERPATH={} | PRMAN_DELEGATE_PLUGIN_PATH={}\n".format(
                log_root,
                log_root,
                os.environ.get("RMAN_SHADERPATH", ""),
                os.environ.get("PRMAN_DELEGATE_PLUGIN_PATH", ""),
            )
        )
        sys.__stdout__.flush()

    return saved_env


def logDiagnostics(stage):
    """Log PRMan-related diagnostics (only when MAYAHYDRA_TEST_DEBUG is set).

    Args:
        stage: Short label printed in the header (e.g. "before renderer switch").
    """
    if not _testDebugEnabled():
        return

    import maya.cmds as cmds

    def _write(msg):
        sys.__stdout__.write(msg + "\n")
        sys.__stdout__.flush()

    _write("PRMan diagnostics [{}]".format(stage))

    keys = [
        "MAYAHYDRA_CODE_COVERAGE",
        "RMANTREE",
        "RENDERMAN_LOCATION",
        "PRMAN_DELEGATE_PLUGIN_PATH",
        "PIXAR_LICENSE_FILE",
        "RMAN_SHADERPATH",
        "RMAN_CONFIG_OVERRIDE",
        "RDIR",
        "CUDA_VISIBLE_DEVICES",
        "CUDA_DEVICE_ORDER",
        "NV_GPU",
    ]
    for key in keys:
        _write("  {}={}".format(key, os.environ.get(key, "")))

    plugin_path = os.environ.get("PRMAN_DELEGATE_PLUGIN_PATH", "")
    if plugin_path:
        sep = ";" if platform.system() == "Windows" else ":"
        for entry in [p for p in plugin_path.split(sep) if p]:
            _write("  PRMAN_DELEGATE_PLUGIN_PATH entry: {} (exists={})".format(
                entry, os.path.isdir(entry)))
            plug_info = os.path.join(entry, "plugInfo.json")
            _write("    plugInfo.json: {} (exists={})".format(
                plug_info, os.path.isfile(plug_info)))

    try:
        _write("  mayaHydra listRenderers: {}".format(cmds.mayaHydra(listRenderers=True)))
    except Exception as e:
        _write("  mayaHydra listRenderers failed: {}".format(e))
    try:
        _write("  mayaHydra listActiveRenderers: {}".format(
            cmds.mayaHydra(listActiveRenderers=True)))
    except Exception as e:
        _write("  mayaHydra listActiveRenderers failed: {}".format(e))
    try:
        _write("  mayaHydra listRegisteredOverrides: {}".format(
            cmds.mayaHydra(listRegisteredOverrides=True)))
    except Exception as e:
        _write("  mayaHydra listRegisteredOverrides failed: {}".format(e))


def shouldRunDelegate(fail_fn=None):
    """Return True if the PRMan delegate should run in this environment.

    Checks the MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS env var, the current
    platform, and whether the HdPrman plugin is registered with mayaHydra.

    Args:
        fail_fn: Preferred way to report a CI-required failure (e.g. pass
                 ``self.fail`` from a ``unittest.TestCase`` for a nicer test
                 failure). When PRMan is required on CI but unavailable the
                 failure is raised regardless of this argument: if ``fail_fn``
                 is None a ``RuntimeError`` is raised so the "required on CI"
                 invariant cannot be silently dropped by an omitted argument.

    Returns:
        True if the PRMan delegate is available and should run.
    """
    allowed = _getAllowedPlatforms()
    if not allowed:
        if _isCiBuild() and platform.system().lower() == "windows":
            _failOnCi(
                fail_fn,
                "PRMan delegate required on CI but "
                "MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS not set.",
            )
        if _testDebugEnabled():
            sys.__stdout__.write(
                "PRMan delegate skipped: MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS not set.\n"
            )
            sys.__stdout__.flush()
        return False

    if platform.system().lower() not in allowed:
        if _testDebugEnabled():
            sys.__stdout__.write(
                "PRMan delegate skipped: unsupported platform {} (allowed={}).\n".format(
                    platform.system(), ",".join(allowed))
            )
            sys.__stdout__.flush()
        return False

    available, renderers = _isRendererAvailable()
    if not available:
        if _isCiBuild():
            _failOnCi(
                fail_fn,
                "PRMan delegate required on CI but renderer not available. "
                "listRenderers={}".format(renderers),
            )
        if _testDebugEnabled():
            sys.__stdout__.write("PRMan delegate skipped: renderer not available.\n")
            sys.__stdout__.flush()
        return False

    return True


# ---------------------------------------------------------------------------
# Private helpers
# ---------------------------------------------------------------------------

def _failOnCi(fail_fn, msg):
    """Report a CI-required failure, raising even when no fail_fn is given."""
    if fail_fn:
        fail_fn(msg)
    raise RuntimeError(msg)


def _getAllowedPlatforms():
    """Return the list of platform names on which PRMan tests are permitted."""
    raw = os.environ.get("MAYAHYDRA_PRMAN_ALLOWED_PLATFORMS", "")
    if not raw:
        return []
    return [p.strip().lower() for p in raw.replace(";", ",").split(",") if p.strip()]


def _isCiBuild():
    """Return True when running under Jenkins/CI."""
    return bool(os.environ.get("JENKINS_URL") or os.environ.get("BUILD_ID"))


def _isRendererAvailable():
    """Return (available, renderers_list) for the PRMan Hydra plugin."""
    import maya.cmds as cmds
    try:
        renderers = cmds.mayaHydra(listRenderers=True) or []
    except Exception as e:
        if _testDebugEnabled():
            sys.__stdout__.write("PRMan availability check failed: {}\n".format(e))
            sys.__stdout__.flush()
        return False, []
    return HD_PRMAN in renderers, renderers


def tearDown(saved_env):
    """Restore the PRMan environment variables saved by setUp().

    Args:
        saved_env: The dict returned by setUp(), or None (no-op).
    """
    for k, v in (saved_env or {}).items():
        if v is None:
            os.environ.pop(k, None)
        else:
            os.environ[k] = v
