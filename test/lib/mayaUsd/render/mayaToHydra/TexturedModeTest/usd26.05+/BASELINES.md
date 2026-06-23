# USD 26.05+ Baselines — TexturedModeTest

| Image | Source |
|-------|--------|
| `untextured.png` | **New in USD 26.05** — updated baseline from failing preflight |
| `textured.png` | Copied from `TexturedModeTest/` root — no visual change in USD 26.05 |

## Why unchanged images are copied here instead of relying on the root folder

The test framework's `resolveRefImage` builds the baseline path as
`<testDir>/<imageVersion>/<imageName>` with no fallback to the parent folder.
If `imageVersion` is set to `"usd26.05+"` and a file is missing from this folder,
the test will fail with a missing file error rather than falling back to the root
`TexturedModeTest/` directory.
Therefore all images that this test compares with `imageVersion` must be present here,
even those whose pixel content is identical to the root version.
