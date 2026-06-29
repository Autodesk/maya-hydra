# USD 26.05+ Baselines — USDLightsTest

| Image | Source |
|-------|--------|
| `allLights.png` | **New in USD 26.05** — updated baseline from failing preflight |
| `domeLight.png` | Copied from `usd25.11/` — no visual change in USD 26.05 |
| `flatLight.png` | Copied from `usd25.11/` — no visual change in USD 26.05 |

## Why unchanged images are copied here instead of relying on usd25.11/

The test framework's `resolveRefImage` builds the baseline path as
`<inputDir>/<imageVersion>/<imageName>` with no fallback to the parent folder.
If `imageVersion` is set to `"usd26.05+"` and a file is missing from this folder,
the test will fail with a missing file error rather than falling back to `usd25.11/`.
Therefore all images that this test compares with `imageVersion` must be present here,
even those whose pixel content is identical to the `usd25.11/` version.
