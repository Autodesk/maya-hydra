# USD 26.05+ Baselines — ArnoldLightsTest

| Image | Why |
|-------|-----|
| `allLights.png` | The USD sphere takes slightly different dome lighting in USD 26.05 |

`aiAreaLight.png` is asserted with no `imageVersion` and matches its root
baseline on every USD version, so it is deliberately absent here.

## About the difference

The same USD sphere dome lighting change as `USDLightsTest/usd26.05+/`. Against
`usd25.11+/allLights.png` the mean error is 8.7e-05, but 806 pixels (0.504%)
land over the 0.01 per-pixel threshold, well past the 0.2% this test allows. It
is deterministic and identical in both selection highlight modes.

## Relation to flash/usd26.05+/

`flash/usd26.05+/allLights.png` is the Flash baseline for the same assert and
wins the lookup on a Flash run. Flash ships for USD 26.05+ only, so that folder
is version-tagged purely to match the `imageVersion` the test passes; it has no
USD 25.11 counterpart.
