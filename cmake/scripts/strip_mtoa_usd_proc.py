#!/usr/bin/env python3
# Copyright 2026 Autodesk
#
# HYDRA-2506 workaround.
#
# Arnold ships a monolithic procedural, "usd_proc.dylib", that statically
# embeds a *private* copy of OpenUSD. When that library is loaded into a Maya
# process that already has USD loaded (as happens during Hydra rendering with
# MtoA on macOS), its static initializers re-run USD's TF_REGISTRY_FUNCTIONs.
# This collides with Maya's already-populated Tf registry and produces:
#
#   Error: TfType 'HdGpGenerativeProceduralPlugin' already has a defined C++ type; cannot redefine
#   Error: Cannot change the factory of HdGpSceneIndexPlugin
#   Error: Plugin HdCustomNodeTranslationSceneIndexPlugin is missing TfType registration
#
# which leaves the maya-hydra scene-index plugin uninstantiated and the render
# producing no image. Per MtoA guidance, "ArnoldUsdBundle.dylib" supersedes
# "usd_proc.dylib" and links against the *shared* USD, so removing the
# standalone "usd_proc.dylib" (and any plugInfo.json reference to it) from an
# extracted mtoa cut avoids the duplicate-USD collision without losing the
# Arnold USD-imaging plugins provided by the bundle.
#
# This script is a build-time developer/CI tool. Its single argument is the
# root of an *already extracted* mtoa cut, supplied by the build system
# (MTOA_LOCATION). It is validated below before any filesystem operation
# (Secure Python Development rule 1: never use argv directly in file ops).

import json
import os
import sys

_USD_PROC_NAME = "usd_proc.dylib"


def _strip_hash_comments(text):
    """Drop full-line '#' comments so USD-style plugInfo.json parses as JSON.

    USD's JSON reader tolerates '#' comments; Python's json module does not.
    We only strip lines whose first non-whitespace character is '#', which is
    the convention usdGenSchema emits, to avoid touching '#' inside strings.
    """
    kept_lines = []
    for line in text.splitlines():
        if line.lstrip().startswith("#"):
            continue
        kept_lines.append(line)
    return "\n".join(kept_lines)


def _strip_usd_proc_from_plugin_info(plugin_info_path):
    """Remove any Plugins[] entry whose LibraryPath resolves to usd_proc.dylib.

    Returns True if the file was modified.
    """
    try:
        with open(plugin_info_path, "r", encoding="utf-8") as handle:
            raw = handle.read()
    except OSError as error:
        print(
            "  [warn] could not read {path}: {err}".format(
                path=plugin_info_path, err=error
            )
        )
        return False

    # Fast path: only files that mention usd_proc are candidates for editing.
    # This also keeps the output quiet for the many unrelated USD manifests.
    if _USD_PROC_NAME not in raw and "usd_proc" not in raw:
        return False

    try:
        data = json.loads(_strip_hash_comments(raw))
    except ValueError as error:
        # This file references usd_proc but is not strict JSON even after
        # stripping '#' comments (e.g. trailing commas). Warn loudly rather
        # than silently leaving a live reference behind.
        print(
            "  [WARN] {path} references usd_proc but could not be parsed to "
            "remove it ({err}); please edit it manually.".format(
                path=plugin_info_path, err=error
            )
        )
        return False

    if not isinstance(data, dict):
        return False

    plugins = data.get("Plugins")
    if not isinstance(plugins, list):
        return False

    kept = []
    removed = 0
    for entry in plugins:
        library_path = entry.get("LibraryPath") if isinstance(entry, dict) else None
        if isinstance(library_path, str) and os.path.basename(library_path) == _USD_PROC_NAME:
            removed += 1
            continue
        kept.append(entry)

    if removed == 0:
        return False

    data["Plugins"] = kept
    with open(plugin_info_path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2)
        handle.write("\n")
    print(
        "  [json] removed {n} usd_proc reference(s) from {path}".format(
            n=removed, path=plugin_info_path
        )
    )
    return True


def strip_usd_proc(mtoa_root):
    """Delete usd_proc.dylib and scrub plugInfo.json references under mtoa_root."""
    deleted_libs = 0
    scrubbed_json = 0

    for dirpath, _dirnames, filenames in os.walk(mtoa_root):
        for filename in filenames:
            full_path = os.path.join(dirpath, filename)
            if filename == _USD_PROC_NAME:
                try:
                    os.remove(full_path)
                    deleted_libs += 1
                    print("  [del]  {path}".format(path=full_path))
                except OSError as error:
                    print(
                        "  [warn] could not delete {path}: {err}".format(
                            path=full_path, err=error
                        )
                    )
            elif filename == "plugInfo.json":
                if _strip_usd_proc_from_plugin_info(full_path):
                    scrubbed_json += 1

    print(
        "strip_mtoa_usd_proc: deleted {libs} usd_proc.dylib file(s), "
        "scrubbed {jsons} plugInfo.json file(s) under {root}".format(
            libs=deleted_libs, jsons=scrubbed_json, root=mtoa_root
        )
    )


def main(argv):
    if len(argv) != 2:
        print("usage: strip_mtoa_usd_proc.py <mtoa_root>", file=sys.stderr)
        return 2

    # Validate the build-supplied path before touching the filesystem
    # (Secure Python Development rule 1). We only ever operate *within* this
    # resolved, existing directory.
    mtoa_root = os.path.realpath(argv[1])
    if not os.path.isdir(mtoa_root):
        print(
            "strip_mtoa_usd_proc: '{root}' is not a directory; nothing to do".format(
                root=mtoa_root
            )
        )
        return 0

    strip_usd_proc(mtoa_root)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
