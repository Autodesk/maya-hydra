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
"""Maya-free, stdlib-only image comparison utilities.

This module is shared between:
  - Viewport tests (run inside mayapy) via test/testUtils/imageUtils.py's
    ImageDiffingTestCase.assertImagesClose().
  - Production-rendering tests (run under system Python / mayapy) via
    test/lib/cmdLineRender/compareRenderedImage.py (Mechanism A) and
    test/lib/cmdLineRender/renderSettingsMultiImageTest.py (Mechanism B).

It intentionally imports nothing from maya.cmds or PySide so it can be
imported cleanly from any Python interpreter used by the above callers.

The single entry point most callers should use is compareImagePair(): it
runs idiff, applies pass/fail policy, generates a visual diff image on
failure, and builds a human-readable failure report (with Jenkins artifact
URLs when available). Lower-level primitives (imageDiff, generateDiffImage,
reportImageComparisonFailure) are exposed for callers that need finer
control (e.g. the viewport ImageDiffingTestCase wrapper).
"""

import os
import pathlib
import platform
import shutil
# subprocess is used to launch OpenImageIO's idiff/oiiotool binaries for
# image comparison. Bandit B404 (PYTH-INJC-30) flags any subprocess import
# as a command-injection candidate, but every subprocess.run() call in this
# module uses the argument-list form (no shell=True) and the tool path is
# either sourced from the IMAGE_DIFF_TOOL/OIIOTOOL environment variables
# (exported by cmake/test.cmake for every test) or explicitly validated by
# the validate_executable() helper below before being passed in by callers.
import subprocess  # nosec B404
import sys
from collections import namedtuple


class ImageComparisonResult(namedtuple(
        "ImageComparisonResult", ["passed", "message", "returncode"])):
    """Result of compareImagePair().

    passed     -- True if the comparison should be treated as a pass.
    message    -- Human-readable failure report (None when passed).
    returncode -- idiff's return code, or None if idiff failed to execute.
    """


# ---------------------------------------------------------------------------
# Argv validators (moved from renderSettingsMultiImageTest.py so both
# Mechanism A and Mechanism B share the same Bandit-safe path checks).
# ---------------------------------------------------------------------------

def validate_executable(label, raw_path):
    """Return a resolved absolute Path for an executable supplied on argv.

    Rejects relative paths (so subprocess.run() can never search PATH for a
    same-named binary), missing files, non-regular files (directories,
    dangling symlinks) and non-executable files. Exits the process with a
    descriptive message on any failure. This is the runtime mitigation for
    Bandit B603 / PYTH-INJC-30 on the subprocess.run() call sites in this
    module and in its callers.
    """
    candidate = pathlib.Path(raw_path)
    if not candidate.is_absolute():
        print("%s path must be absolute: %s" % (label, raw_path), file=sys.stderr)
        sys.exit(1)
    try:
        resolved = candidate.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("%s not found: %s (%s)" % (label, raw_path, exc), file=sys.stderr)
        sys.exit(1)
    if not resolved.is_file():
        print("%s is not a regular file: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    if not os.access(str(resolved), os.X_OK):
        print("%s is not executable: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    return resolved


def validate_existing_file(label, raw_path):
    """Resolve a required input file path, exiting on any validation failure."""
    candidate = pathlib.Path(raw_path)
    try:
        resolved = candidate.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("%s not found: %s (%s)" % (label, raw_path, exc), file=sys.stderr)
        sys.exit(1)
    if not resolved.is_file():
        print("%s is not a regular file: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    return resolved


def validate_existing_dir(label, raw_path):
    """Resolve a required input directory path, exiting on any failure."""
    candidate = pathlib.Path(raw_path)
    try:
        resolved = candidate.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("%s not found: %s (%s)" % (label, raw_path, exc), file=sys.stderr)
        sys.exit(1)
    if not resolved.is_dir():
        print("%s is not a directory: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    return resolved


def validate_float(label, raw_value):
    """Parse a numeric threshold supplied on argv, exiting on failure."""
    try:
        return float(raw_value)
    except ValueError:
        print("%s must be a number, got: %s" % (label, raw_value), file=sys.stderr)
        sys.exit(1)


# ---------------------------------------------------------------------------
# Environment helpers
# ---------------------------------------------------------------------------

def _subprocessEnv():
    """Build the environment to use when spawning idiff/oiiotool.

    On Linux, swap LD_LIBRARY_PATH for IDIFF_LD_LIBRARY_PATH (when set) so
    idiff/oiiotool load compatible libs instead of Maya's libpng. This env
    var is exported per-test by cmake/test.cmake; the comment there notes
    that the Python subprocess launching idiff must perform this swap.
    """
    env = os.environ.copy()
    if platform.system() != "Windows":
        idiffLdLibraryPath = env.get("IDIFF_LD_LIBRARY_PATH")
        if idiffLdLibraryPath:
            env["LD_LIBRARY_PATH"] = idiffLdLibraryPath
    return env


# ---------------------------------------------------------------------------
# Primitives
# ---------------------------------------------------------------------------

def imageDiff(baselinePath, actualPath, verbose, fail, failpercent, hardfail=None,
              failrelative=None, warn=None, warnpercent=None, hardwarn=None,
              perceptual=False, imageDiffTool=None):
    """ Returns the completed process instance after running idiff or None if
        execution of process failed.

    baselinePath -- Baseline (expected) image to compare.
    actualPath   -- Actual (rendered/captured) image to compare.
    verbose      -- If enabled, the image diffing command will be printed to log.
    fail         -- The threshold for absolute pixel difference for failure.
    failpercent  -- The percentage of pixels that can be different before failure.
    failrelative -- If set, uses relative difference (scaled by mean of two values). Use 0 for
                    strict pixel-per-pixel absolute comparison only.
    hardfail     -- Triggers a failure if any pixels are above this threshold (if the absolute
                    difference is below this threshold).
    warn         -- The threshold for the acceptable difference of a pixel for a warning.
    warnpercent  -- The percentage of pixels that can be different before a warning.
    hardwarn     -- Triggers a warning if any pixels are above this threshold.
    perceptual   -- Performs an additional test to see if two images are visually different.
                    If enabled, test overall will fail if more than the "fail percentage" failed
                    the perceptual test.
    imageDiffTool -- Optional explicit path to the idiff binary. Falls back to the
                    IMAGE_DIFF_TOOL environment variable when omitted.

    By default, if any pixels differ between the images, the comparison will fail.
    If, for example, we set fail=0.004, failpercent=10 and hardfail=0.25, the comparison will
    fail if more than 10% of the pixels differ by 0.004, or if any pixel differs by more than
    0.25 (just above a 1/255 threshold).

    For more information, see https://github.com/OpenImageIO/oiio/blob/cb6475c0dd72b9c49d862d98c6cd2da4509d5f37/src/doc/idiff.rst#L1
    """
    tool = imageDiffTool or os.environ['IMAGE_DIFF_TOOL']

    cmdArgs = []
    if warn is not None:
        cmdArgs.extend(['-warn', str(warn)])
    if warnpercent is not None:
        cmdArgs.extend(['-warnpercent', str(warnpercent)])
    if hardwarn is not None:
        cmdArgs.extend(['-hardwarn', str(hardwarn)])
    if fail is not None:
        cmdArgs.extend(['-fail', str(fail)])
    if failpercent is not None:
        cmdArgs.extend(['-failpercent', str(failpercent)])
    if failrelative is not None:
        cmdArgs.extend(['-failrelative', str(failrelative)])
    if hardfail is not None:
        cmdArgs.extend(['-hardfail', str(hardfail)])
    if perceptual:
        cmdArgs.extend(['-p'])
    cmd = [tool]
    cmd.extend(cmdArgs)
    # idiff takes the first image as the reference ("baseline first").
    cmd.extend([baselinePath, actualPath])

    if verbose:
        sys.__stdout__.write("\nimage diffing with {0}".format(cmd))
        sys.__stdout__.flush()

    # Run idiff command
    #
    # On some Windows 11 machines we were randomly getting a failure when
    # launching the subprocess.run().
    #   OSError: [WinError 50] The request is not supported
    #
    # The cause appeared to come from the subprocess.run() call where it
    # was only capturing stdout. In subprocess the error occured when trying
    # to duplicate the stderr handle.
    #proc = subprocess.run(cmd, shell=False, env=os.environ.copy(), stdout=subprocess.PIPE)
    # When using flag 'capture_output=True' to capture both (stdout/stderr) the
    # random error disappeared.
    #
    # On Windows 11 24H2 (Windows Terminal as the default console host),
    # launching a console child like idiff.exe from a Maya UI process (which
    # has no inherited console) hangs subprocess.run forever -- Windows
    # fails the console-pipe handshake with ERROR_NO_DATA (0x800700E8).
    # CREATE_NO_WINDOW skips that handshake.
    #
    # As of 2026-09-22, Windows-only hangs in subprocess.run() are still
    # occuring for unit test runs on build machines.  We have implemented a
    # timeout and retry mechanism to compensate for this.  If the problem
    # persists, the following suggestions can be tried:
    #
    # - This is the only subprocess.run() call site in this module that
    #   implements mitigation measures against hangs.  We could factor out
    #   the code below and have all call sites use it.  For example,
    #   generateDiffImage()'s subprocess.run() calls do not use hang
    #   mitigation, though it seems unlikely that this is a problem, as this
    #   is reached only after an image comparison failure and is used for
    #   visualization only.
    #
    #   A call site in test/testUtils/mtohUtils.py runs the taskkill
    #   executable, and does not use the code below either.  There has been at
    #   least one recorded instance of a test run of testSceneStat.py hanging,
    #   and that test performs no image comparison whatsoever (the automated
    #   retry of the complete test succeeded).  It seems possible that this
    #   hang might have been caused by the mtohUtils.py subprocess.run()
    #   invocation.
    #
    # - Microsoft documents
    #   https://learn.microsoft.com/en-us/windows/win32/ipc/pipe-handle-inheritance
    #   that all processes holding a pipe handle must close it for the pipe to
    #   reach End Of File.  If a subprocess started by subprocess.run(), or a
    #   subprocess of that process, holds on to a pipe without closing it,
    #   subprocess.run() will hang on Windows.  The capture_output=True
    #   argument means both stdout and stderr are anonymous pipes.  For best
    #   robustness, redirecting stdout and stderr to temporary files means
    #   subprocess.run() will return when the child process completes, not when
    #   the pipes are closed.  The temporary files can then be read for output.
    #
    #   However, idiff itself does not spawn subprocesses (from inspection of
    #   its open source code).  Unless idiff itself hangs, it will not hold on
    #   to stdout / stderr pipes, so the likelihood that using temporary files
    #   will reduce the occurrence of hangs seems low.
    #
    #   The taskkill executable is closed source, and it might create one or
    #   more subprocess(es), and any of these processes might hold on to stdout
    #   / stderr, or taskkill itself may hang, but this seems unlikely as well.
    creation_flags = subprocess.CREATE_NO_WINDOW if platform.system() == "Windows" else 0

    timeoutSeconds = 20
    maxAttempts = 3

    for attempt in range(1, maxAttempts + 1):
        try:
            # https://github.com/python/cpython/issues/88693#issuecomment-3177334016
            # suggests setting stdin to DEVNULL to avoid hanging waiting on
            # stdin, though for idiff this seems unlikely.
            #
            # cmd's first element (the idiff/imageDiffTool binary) is sourced
            # either from the IMAGE_DIFF_TOOL environment variable (exported by
            # cmake/test.cmake for every test) or from an explicit
            # imageDiffTool argument validated by validate_executable() in
            # callers. The argument-list form (shell=False) is used
            # throughout, so this satisfies Bandit B603 / PYTH-INJC-30.
            proc = subprocess.run(  # nosec B603
                cmd,
                stdin=subprocess.DEVNULL,
                capture_output=True,
                shell=False,
                text=True,
                env=_subprocessEnv(),
                creationflags=creation_flags,
                timeout=timeoutSeconds,
            )
        except subprocess.TimeoutExpired:
            sys.__stderr__.write(
                '\nWarning: imageDiff timed out after {0} seconds '
                '(attempt {1} of {2}): {3}'.format(timeoutSeconds, attempt,
                                                    maxAttempts, cmd))
            sys.__stderr__.flush()
        except OSError as e:
            # If its not the random WinError 50 we re-raise it.
            if '[WinError 50]' not in str(e):
                raise

            if verbose:
                sys.__stdout__.write('\nimageDiff failed with: {0}'.format(str(e)))
                sys.__stdout__.flush()
            break
        else:
            # Successfully executed imageDiff.
            return proc

    return None  # Running of imageDiff failed.


def generateDiffImage(baselinePath, actualPath, outputPath, imageDiffTool=None):
    """Generate a visual diff image. Prefers oiiotool --absdiff --maxchan; falls back to idiff.

    This is used for visualization only on comparison failure -- it never affects the
    pass/fail decision, which is made solely by imageDiff()/idiff.

    imageDiffTool -- Optional explicit idiff path, used both to locate a sibling oiiotool
                      binary and as the idiff fallback. Falls back to the IMAGE_DIFF_TOOL
                      environment variable when omitted, so the pass/fail tool and the
                      diff-image tool cannot diverge when a caller passes an explicit binary.

    Returns output path if successful, else None."""
    tool = imageDiffTool or os.environ.get('IMAGE_DIFF_TOOL')
    if not tool:
        return None
    os.makedirs(os.path.dirname(outputPath), exist_ok=True)

    env = _subprocessEnv()

    # Prefer oiiotool: --absdiff --maxchan --powc 0.5 --mulc 40 --clamp:max=1
    oiiotool_exe = (
        os.environ.get('OIIOTOOL') or
        (os.path.join(os.path.dirname(tool),
          'oiiotool.exe' if sys.platform == 'win32' else 'oiiotool')
         if os.path.dirname(tool) else None) or
        (shutil.which('oiiotool') if shutil.which else None)
    )
    if oiiotool_exe and os.path.isfile(oiiotool_exe):
        cmd = [
            oiiotool_exe, baselinePath, actualPath,
            '--absdiff', '--maxchan', '--powc', '0.5', '--mulc', '40',
            '--clamp:max=1', '-o', outputPath
        ]
        oiio_env = dict(env)
        oiio_dir = os.path.dirname(oiiotool_exe)
        if oiio_dir:
            oiio_env['PATH'] = oiio_dir + os.pathsep + oiio_env.get('PATH', '')
        try:
            # oiiotool_exe is sourced from the OIIOTOOL environment variable
            # (exported by cmake/test.cmake) or derived from the validated
            # idiff tool path; argument-list form (shell=False) is used, so
            # this satisfies Bandit B603 / PYTH-INJC-30.
            proc = subprocess.run(  # nosec B603
                cmd, capture_output=True, shell=False, env=oiio_env)
            if proc.returncode == 0 and os.path.isfile(outputPath):
                return outputPath
        except OSError:
            pass

    # Fallback: idiff -o -abs -scale 1
    cmd = [tool, '-o', outputPath, '-abs', '-scale', '1', baselinePath, actualPath]
    try:
        # tool is sourced from IMAGE_DIFF_TOOL / the validated imageDiffTool
        # argument; argument-list form (shell=False) is used, so this
        # satisfies Bandit B603 / PYTH-INJC-30.
        proc = subprocess.run(cmd, capture_output=True, shell=False, env=env)  # nosec B603
        if proc.returncode in (0, 1, 2) and os.path.isfile(outputPath):
            return outputPath
    except OSError:
        pass
    return None


def reportImageComparisonFailure(baselinePath, actualPath, idiffStdout, idiffStderr=None,
                                  diffOutputDir=None, imageDiffTool=None):
    """Build a human-readable failure report for a failed image comparison.

    Does not raise/fail anything itself -- callers decide how to surface the
    returned string (self.fail(), print to stderr, etc). Generates a visual
    diff image as a side effect (for visualization only; does not affect
    pass/fail).

    baselinePath  -- Baseline (expected) image path.
    actualPath    -- Actual (rendered/captured) image path.
    idiffStdout   -- idiff's captured stdout, prefixed to the message.
    idiffStderr   -- idiff's captured stderr, appended when non-empty.
    diffOutputDir -- Directory to write the diff image to. Defaults to
                     dirname(actualPath) when omitted.
    imageDiffTool -- Optional explicit idiff path, forwarded to generateDiffImage().
    """
    abs1 = os.path.abspath(baselinePath).replace('\\', '/')
    abs2 = os.path.abspath(actualPath).replace('\\', '/')

    _, ext1 = os.path.splitext(os.path.basename(abs1))
    base2, ext2 = os.path.splitext(os.path.basename(abs2))
    ext = ext1 or ext2 or '.png'

    if diffOutputDir is None:
        diffOutputDir = os.path.dirname(abs2)
    else:
        diffOutputDir = str(diffOutputDir)
    diff_output = os.path.join(diffOutputDir, base2 + '_diff' + ext)
    diff_path = generateDiffImage(abs1, abs2, diff_output, imageDiffTool=imageDiffTool)

    artifact_base = os.environ.get('JENKINS_ARTIFACT_BASE', '').rstrip('/')
    # Prefer explicit workspace; fall back to WORKSPACE (Jenkins); then infer from paths
    workspace = os.environ.get('JENKINS_ARTIFACT_WORKSPACE', '') or os.environ.get('WORKSPACE', '')
    if workspace:
        workspace = os.path.normpath(workspace)

    msg = str(idiffStdout) + "\n\nImage comparison failed.\n"
    if idiffStderr:
        msg += str(idiffStderr) + "\n"

    if artifact_base:
        def _resolve(p):
            try:
                return str(pathlib.Path(p).resolve())
            except (OSError, RuntimeError):
                return os.path.normpath(p.replace('/', os.sep))

        def _artifact_url(abs_path, workspace_dir):
            if not workspace_dir:
                return None
            try:
                resolved_path = _resolve(abs_path)
                resolved_ws = _resolve(workspace_dir)
                rel = os.path.relpath(resolved_path, resolved_ws)
                if rel.startswith('..'):
                    return None
                return artifact_base + '/' + rel.replace('\\', '/')
            except ValueError:
                return None

        # Try workspace first; if relpath fails (e.g. different drives on Windows),
        # use common ancestor of both image paths as fallback workspace
        url1 = _artifact_url(abs1, workspace) if workspace else None
        url2 = _artifact_url(abs2, workspace) if workspace else None
        if (url1 is None or url2 is None) and abs1 and abs2:
            try:
                r1 = _resolve(abs1)
                r2 = _resolve(abs2)
                common = os.path.commonpath([r1, r2])
                if common:
                    if url1 is None:
                        url1 = _artifact_url(abs1, common)
                    if url2 is None:
                        url2 = _artifact_url(abs2, common)
            except (ValueError, OSError):
                pass

        browse_url = artifact_base + '/'

        url_diff = None
        if diff_path:
            diff_abs = os.path.abspath(diff_path).replace('\\', '/')
            url_diff = _artifact_url(diff_abs, workspace) if workspace else None
            if url_diff is None and abs1 and abs2:
                try:
                    r1 = _resolve(abs1)
                    r2 = _resolve(abs2)
                    common = os.path.commonpath([r1, r2])
                    if common:
                        url_diff = _artifact_url(diff_abs, common)
                except (ValueError, OSError):
                    pass

        if url1:
            msg += "  Baseline: {}\n".format(url1)
        if url2:
            msg += "  Actual:   {}\n".format(url2)
        if url_diff:
            msg += "  Diff:     {}\n".format(url_diff)
        msg += "  Browse:   {}\n".format(browse_url)
    else:
        msg += "  Baseline: {}\n".format(abs1)
        msg += "  Actual:   {}\n".format(abs2)
        if diff_path:
            msg += "  Diff:     {}\n".format(os.path.abspath(diff_path).replace('\\', '/'))

    return msg


# ---------------------------------------------------------------------------
# Orchestrator
# ---------------------------------------------------------------------------

def compareImagePair(baselinePath, actualPath, fail, failpercent, *, hardfail=None,
                      failrelative=None, warn=None, warnpercent=None, hardwarn=None,
                      perceptual=False, imageDiffTool=None, diffOutputDir=None,
                      passReturnCodes=(0,), verbose=False):
    """Compare two images end-to-end: idiff -> pass/fail policy -> diff image -> report.

    Single entry point for all callers (viewport, Mechanism A, Mechanism B); callers
    should not call imageDiff() + reportImageComparisonFailure() directly unless they
    need low-level control.

    baselinePath, actualPath -- Images to compare, baseline first.
    fail, failpercent        -- idiff failure thresholds (see imageDiff()).
    hardfail, failrelative, warn, warnpercent, hardwarn, perceptual
                              -- forwarded to imageDiff() unchanged (see imageDiff()).
    imageDiffTool             -- Optional explicit idiff path, forwarded to imageDiff()
                                  and generateDiffImage().
    diffOutputDir             -- Directory to write the diff image to on failure.
                                  Forwarded to reportImageComparisonFailure(); defaults
                                  to dirname(actualPath) when omitted.
    passReturnCodes           -- Tuple of idiff exit codes that count as a pass.
                                  Default (0,): only an exact match passes, both
                                  warning (rc 1) and error (rc >= 2) fail. Viewport's
                                  assertImagesClose() passes (0, 1) to keep its
                                  historical lenient behaviour (rc 1 = pass).
    verbose                   -- Forwarded to imageDiff() (prints the idiff command).

    Returns an ImageComparisonResult(passed, message, returncode).
    """
    # HYDRA-2304 warn/fail mirroring: when warn/warnpercent are omitted, default them
    # to fail/failpercent -- but only when fail is not None (so assertImagesEqual,
    # which passes fail=None, failpercent=None, keeps its "no thresholds / idiff
    # default" behaviour). Callers that pass explicit warn= keep current behaviour.
    if fail is not None:
        if warn is None:
            warn = fail
        if warnpercent is None:
            warnpercent = failpercent

    proc = imageDiff(
        baselinePath, actualPath, verbose=verbose, fail=fail, failpercent=failpercent,
        hardfail=hardfail, failrelative=failrelative, warn=warn, warnpercent=warnpercent,
        hardwarn=hardwarn, perceptual=perceptual, imageDiffTool=imageDiffTool,
    )

    if proc is None:
        return ImageComparisonResult(
            passed=False, message="Failed to execute imageDiff", returncode=None)

    if proc.returncode in passReturnCodes:
        return ImageComparisonResult(passed=True, message=None, returncode=proc.returncode)

    message = reportImageComparisonFailure(
        baselinePath, actualPath, proc.stdout, idiffStderr=proc.stderr,
        diffOutputDir=diffOutputDir, imageDiffTool=imageDiffTool,
    )
    return ImageComparisonResult(passed=False, message=message, returncode=proc.returncode)
