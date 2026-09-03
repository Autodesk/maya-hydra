#!/usr/bin/env python3
# Copyright 2026 Autodesk, Inc. All rights reserved.
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
import shlex
import shutil
# subprocess is required to launch Maya's Render executable and OpenImageIO's
# idiff binary for this CTest unit test. Bandit B404 (PYTH-INJC-30) flags any
# subprocess import as a command-injection candidate, but in this script all
# argv entries are supplied by CTest itself (see
# test/lib/cmdLineRender/CMakeLists.txt) and every subprocess.run() call uses
# the argument list form without shell=True, so the shell is never invoked
# and no command-injection vector exists. The two executable paths are also
# validated below to be absolute paths to existing executable regular files
# before they are passed to subprocess.run().
import subprocess  # nosec B404
import sys
import tempfile
from pathlib import Path


DEFAULT_RENDERED_IMAGE_SUBDIR = "projects/default/images"


def _validate_executable(label, raw_path):
    """Return a resolved absolute Path for an executable supplied on argv.

    Rejects relative paths (so subprocess.run() can never search PATH for a
    same-named binary), missing files, non-regular files (directories,
    dangling symlinks) and non-executable files. Exits the process with a
    descriptive message on any failure. This is the runtime mitigation for
    Bandit B603 / PYTH-INJC-30 on the subprocess.run() call sites below.
    """
    candidate = Path(raw_path)
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


def _validate_existing_file(label, raw_path):
    """Resolve a required input file path, exiting on any validation failure."""
    candidate = Path(raw_path)
    try:
        resolved = candidate.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("%s not found: %s (%s)" % (label, raw_path, exc), file=sys.stderr)
        sys.exit(1)
    if not resolved.is_file():
        print("%s is not a regular file: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    return resolved


def _validate_existing_dir(label, raw_path):
    """Resolve a required input directory path, exiting on any failure."""
    candidate = Path(raw_path)
    try:
        resolved = candidate.resolve(strict=True)
    except (FileNotFoundError, OSError) as exc:
        print("%s not found: %s (%s)" % (label, raw_path, exc), file=sys.stderr)
        sys.exit(1)
    if not resolved.is_dir():
        print("%s is not a directory: %s" % (label, resolved), file=sys.stderr)
        sys.exit(1)
    return resolved


def _validate_float(label, raw_value):
    """Parse a numeric threshold supplied on argv, exiting on failure."""
    try:
        return float(raw_value)
    except ValueError:
        print("%s must be a number, got: %s" % (label, raw_value), file=sys.stderr)
        sys.exit(1)

#Is used because we use relative paths render products for rendered images
def _copy_scene_and_usd(scene_path, work_dir):
    scene_path = Path(scene_path)
    work_dir = Path(work_dir)
    work_dir.mkdir(parents=True, exist_ok=True)

    scene_copy = work_dir / scene_path.name
    shutil.copy2(scene_path, scene_copy)

    for usd_file in scene_path.parent.iterdir():
        if usd_file.suffix in [".usd", ".usda", ".usdc", ".usdz"]:
            shutil.copy2(usd_file, work_dir / usd_file.name)

    return scene_copy


def _dump_render_capture(result, label="Render"):
    """Echo a captured Render subprocess result to stderr for CTest logs."""
    print(f"{label} exit code: {result.returncode}", file=sys.stderr)
    if result.stdout:
        print(f"{label} stdout:", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
    if result.stderr:
        print(f"{label} stderr:", file=sys.stderr)
        print(result.stderr, file=sys.stderr)


def _run_render(render_exe, renderer, scene_copy, work_dir, extra_renderer_args=None):
    # render_exe has been validated by _validate_executable() in main(): it
    # is an absolute path to an existing executable regular file. Combined
    # with the argument list form (no shell=True), this satisfies Bandit
    # B603 / PYTH-INJC-30.
    cmd = [str(render_exe), "-renderer", renderer]
    if extra_renderer_args:
        # Use shlex.split() to properly handle quoted arguments (e.g., paths with
        # spaces) and escaped whitespace, rather than naive str.split().
        cmd.extend(shlex.split(extra_renderer_args))
    # Scene file must be last; Render expects: Render -renderer Plugin [OPTIONS] sceneFile
    cmd.append(str(scene_copy))
    print(f"Render command: {cmd}", file=sys.stderr)
    result = subprocess.run(  # nosec B603
        cmd,
        cwd=str(work_dir),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Render failed:", file=sys.stderr)
        _dump_render_capture(result)
    return result


def _find_output_match(expected_path, output_dir):
    output_path = output_dir / expected_path.name
    return output_path if output_path.exists() else None


def _compare_images(idiff, fail, failpercent, expected_dir, output_dir):
    expected_dir = Path(expected_dir)
    output_dir = Path(output_dir)

    expected_images = sorted(expected_dir.glob("*"))
    expected_images = [p for p in expected_images if p.is_file()]
    if not expected_images:
        print(f"No expected images found in {expected_dir}", file=sys.stderr)
        return False

    success = True
    for expected in expected_images:
        output = _find_output_match(expected, output_dir)
        if not output:
            print(f"Missing output image for baseline: {expected}", file=sys.stderr)
            success = False
            continue

        # idiff has been validated by _validate_executable() in main(): it
        # is an absolute path to an existing executable regular file.
        # Combined with the argument list form (no shell=True), this
        # satisfies Bandit B603 / PYTH-INJC-30.
        # Match single-image cmdLineRender tests (HYDRA-2304): treat WARN like FAIL
        # so idiff's default ~1e-6 warning threshold does not fail "roughly same"
        # images that already pass -fail / -failpercent.
        result = subprocess.run(  # nosec B603
            [
                str(idiff),
                "-fail",
                str(fail),
                "-failpercent",
                str(failpercent),
                "-warn",
                str(fail),
                "-warnpercent",
                str(failpercent),
                str(output),
                str(expected),
            ],
            capture_output=True,
            text=True,
        )
        if result.returncode != 0:
            print(f"idiff failed for {expected.name}:", file=sys.stderr)
            print(result.stdout, file=sys.stderr)
            print(result.stderr, file=sys.stderr)
            success = False

    expected_names = {p.name for p in expected_images}
    output_files = sorted(p for p in output_dir.iterdir() if p.is_file())
    unexpected = [p for p in output_files if p.name not in expected_names]
    if unexpected:
        print("Unexpected output images found (output has files not in "
              "expected directory):", file=sys.stderr)
        for u in unexpected:
            print(f"  {u.name}", file=sys.stderr)
        success = False

    return success


def _prepare_output_dir(base_dir, rendered_subdir):
    output_dir = Path(base_dir) / rendered_subdir
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    return output_dir


def _find_output_dir(base_dir, rendered_subdir):
    output_dir = Path(base_dir) / rendered_subdir
    return output_dir


def _dump_plugin_path_env():
    """Echo USD plugin discovery env vars for CTest logs (HYDRA-2506)."""
    for name in (
        "PXR_PLUGINPATH_NAME",
        "MAYA_PXR_PLUGINPATH_NAME",
        "ARNOLD_PLUGIN_PATH",
    ):
        value = os.environ.get(name)
        print(f"{name}={value or ''}", file=sys.stderr)


def _dump_base_dir_listing(base_dir):
    """Recursively list base_dir with file sizes.

    Separates "nothing was written at all" from "written to a different
    directory" and from "written under a different name or extension",
    which a single "Missing output image" line cannot distinguish. Scoped to
    one test's own MAYA_APP_DIR-derived directory, so it stays small.
    """
    base_dir = Path(base_dir)
    print(f"Recursive listing of {base_dir}:", file=sys.stderr)
    if not base_dir.exists():
        print("  (directory does not exist)", file=sys.stderr)
        return
    for path in sorted(base_dir.rglob("*")):
        if path.is_file():
            print(f"  {path} ({path.stat().st_size} bytes)", file=sys.stderr)


def main(argv):
    if len(argv) < 8:
        print(
            "Usage: renderSettingsMultiImageTest.py <RenderExe> <Renderer> "
            "<ScenePath> <ExpectedImagesDir> <IdiffPath> <Fail> <FailPercent> "
            "[RenderedImageSubdir] [RendererArgs]",
            file=sys.stderr,
        )
        return 1

    # Validate every argv entry up front so that downstream code (and in
    # particular the two subprocess.run() calls below) only ever sees
    # well-formed, existing paths and parsed numeric values. This is the
    # runtime mitigation for Bandit B603 / PYTH-INJC-30.
    render_exe   = _validate_executable("Render executable", argv[1])
    renderer     = argv[2]
    scene_path   = _validate_existing_file("Scene path", argv[3])
    expected_dir = _validate_existing_dir("Expected images directory", argv[4])
    idiff        = _validate_executable("idiff executable", argv[5])
    fail         = _validate_float("fail threshold", argv[6])
    failpercent  = _validate_float("failpercent threshold", argv[7])
    rendered_subdir = argv[8] if len(argv) > 8 and argv[8] else DEFAULT_RENDERED_IMAGE_SUBDIR
    extra_renderer_args = argv[9] if len(argv) > 9 else None

    base_dir = Path(os.environ.get("MAYA_APP_DIR") or tempfile.gettempdir())
    work_dir = base_dir / "projects" / "default"
    work_dir.mkdir(parents=True, exist_ok=True)

    print(f"MAYA_APP_DIR={base_dir}", file=sys.stderr)
    maya_render_desc_path = os.environ.get("MAYA_RENDER_DESC_PATH")
    if maya_render_desc_path:
        print(f"MAYA_RENDER_DESC_PATH={maya_render_desc_path}", file=sys.stderr)
    _dump_plugin_path_env()

    scene_copy = _copy_scene_and_usd(scene_path, work_dir)
    _prepare_output_dir(base_dir, rendered_subdir)

    render_result = _run_render(
        render_exe, renderer, scene_copy, work_dir, extra_renderer_args
    )
    if render_result.returncode != 0:
        _dump_base_dir_listing(base_dir)
        return 1

    output_dir = _find_output_dir(base_dir, rendered_subdir)
    if not output_dir.exists():
        print(f"Output Images directory not found: {output_dir}", file=sys.stderr)
        _dump_render_capture(render_result)
        _dump_base_dir_listing(base_dir)
        return 1

    if _compare_images(
        idiff,
        fail,
        failpercent,
        expected_dir,
        output_dir,
    ):
        return 0

    # Render can exit 0 while writing no images (HYDRA-2506). Echo the
    # captured batch output and temp-dir listing so CTest logs stay actionable.
    print(
        "Render completed successfully but image comparison failed:",
        file=sys.stderr,
    )
    _dump_render_capture(render_result)
    _dump_base_dir_listing(base_dir)
    return 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
