# macOS Flash Baselines — FlowViewportAPITest

AGP-1068: on macOS, the Flash renderer blends translucent geometry differently
than it does on Windows and Linux. Geometry and camera framing are identical, but
the cube grids and the filtered sphere come out with the wrong color and opacity:
the translucent red grid of `multipleNodes_BeforeModifs.png`, for instance, stays
saturated here while the more opaque blue grid washes out to white, the opposite
of the shared `flash/` baseline.

These baselines capture that incorrect appearance so the test can run on macOS.
    10|Regenerate them from `flash/` once AGP-1068 is fixed.

## Which images live here

Every image in this folder is one where translucent colored geometry is visible.
The images that `testFlowViewportAPI` captures with no such geometry on screen
(the hidden, deleted and Viewport 2.0 states) match their shared baseline on
macOS and are deliberately absent, so they keep resolving to `flash/` or to the
root `FlowViewportAPITest/` folder.

`usd_2411+/` holds the macOS variant of the one image that also has a USD-version
    20|baseline. `macOSFlashImageVersion()` nests an incoming imageVersion under `osx`,
which is how that image reaches `flash/osx/usd_2411+/`.

## Adding an image here

The test selects this folder per image, by passing `self.macOSFlashImageVersion()`
as the `imageVersion` of the corresponding assert (see `mtohUtils.py`). Note that
`resolveRefImage` has no fallback to the parent folder: an image asserted with
this `imageVersion` but missing from this folder fails with a missing file error
rather than falling back to `FlowViewportAPITest/flash/`.
