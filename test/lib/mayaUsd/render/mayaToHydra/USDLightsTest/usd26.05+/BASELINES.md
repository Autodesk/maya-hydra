# USD 26.05+ Baselines — USDLightsTest

| Image | Why |
|-------|-----|
| `domeLight.png` | The USD sphere takes slightly different dome lighting in USD 26.05 |

Every other image this test captures matches its `usd25.11+` or root baseline on
USD 26.05 and is deliberately absent, so it keeps resolving to those folders.

## About the difference

The change is confined to the USD sphere and is close to invisible: against
`usd25.11+/domeLight.png` the mean error is 9.1e-05, but 858 pixels (0.536%)
still land over the 0.01 per-pixel threshold, just past the 0.5% the test
allows. It is deterministic and identical in both selection highlight modes.

## Adding an image here

The test passes `domeImageVersion` only to the `domeLight` assert. An image
needs its own branch in `testUSDLights.py` before it can live here, and
`resolveRefImage` has no fallback to the parent folder: an image asserted with
this `imageVersion` but missing from this folder fails with a missing file
error rather than falling back to `usd25.11+/`.
