#!/usr/bin/env python3
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

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


def _run_render(render_exe, renderer, scene_copy, work_dir):
    result = subprocess.run(
        [render_exe, "-renderer", renderer, str(scene_copy)],
        cwd=str(work_dir),
        capture_output=True,
        text=True,
    )
    if result.returncode != 0:
        print("Render failed:", file=sys.stderr)
        print(result.stdout, file=sys.stderr)
        print(result.stderr, file=sys.stderr)
    return result.returncode == 0


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

        result = subprocess.run(
            [
                idiff,
                "-fail",
                str(fail),
                "-failpercent",
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

    return success


def _prepare_output_dir(work_dir):
    output_dir = Path(work_dir) / "images"
    if output_dir.exists():
        shutil.rmtree(output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    return output_dir


def _find_output_dir(work_dir):
    output_dir = Path(work_dir) / "images"
    return output_dir


def main(argv):
    if len(argv) < 8:
        print(
            "Usage: renderSettingsMultiImageTest.py <RenderExe> <Renderer> "
            "<ScenePath> <ExpectedImagesDir> <IdiffPath> <Fail> <FailPercent>",
            file=sys.stderr,
        )
        return 1

    render_exe = argv[1]
    renderer = argv[2]
    scene_path = argv[3]
    expected_dir = argv[4]
    idiff = argv[5]
    fail = argv[6]
    failpercent = argv[7]

    base_dir = Path(os.environ.get("MAYA_APP_DIR") or tempfile.gettempdir())
    work_dir = base_dir / "projects" / "default"
    work_dir.mkdir(parents=True, exist_ok=True)

    scene_copy = _copy_scene_and_usd(scene_path, work_dir)
    _prepare_output_dir(work_dir)

    if not _run_render(render_exe, renderer, scene_copy, work_dir):
        return 1

    output_dir = _find_output_dir(work_dir)
    if not output_dir.exists():
        print(f"Output Images directory not found: {output_dir}", file=sys.stderr)
        return 1

    return (
        0
        if _compare_images(
            idiff,
            fail,
            failpercent,
            expected_dir,
            output_dir,
        )
        else 1
    )


if __name__ == "__main__":
    sys.exit(main(sys.argv))
