# macOS Flash Baselines — StagePayloadsReferencesTest

| Image | Source |
|-------|--------|
| `payloadSceneLoadedPotA.png` | **New for macOS Flash** — the flower pot renders white instead of teal (AGP-1068) |

## Why this image differs from `StagePayloadsReferencesTest/flash/`

AGP-1068: on macOS, the Flash renderer blends translucent geometry differently
than it does on Windows and Linux, which here costs the flower pot its color. The
    10|baseline captures that incorrect appearance so the test can run on macOS;
regenerate it from `flash/` once AGP-1068 is fixed. Windows and Linux keep using
the `StagePayloadsReferencesTest/flash/` image.

Only images whose macOS appearance differs need to live here. The test selects
this folder per image, by passing `self.macOSFlashImageVersion()` as the
`imageVersion` of the corresponding assert (see `mtohUtils.py`).

Note that `resolveRefImage` has no fallback to the parent folder: an image
asserted with this `imageVersion` but missing from this folder fails with a
    20|missing file error rather than falling back to
`StagePayloadsReferencesTest/flash/`.
