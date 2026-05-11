<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Batch Render Settings

MayaHydra supports batch rendering through the `hydraRender` command.  When a
batch render is initiated, MayaHydra must decide how to obtain the render
configuration — resolution, camera, output paths, AOVs (arbitrary output
variables), and other render settings.  Three strategies are available, and the
appropriate one is selected automatically based on the render delegate in use
and the contents of the scene.

## Maya Render Settings

This is the traditional approach used by Maya renderers.  Render configuration
is read from standard Maya nodes:

- **defaultRenderGlobals** — animation range, image format, file naming.
- **defaultResolution** — image width and height.
- **Renderable cameras** — one or more Maya cameras marked as renderable.

The batch renderer translates these Maya settings into Hydra task controller
parameters, then manages the full render loop: it sets up the Hydra render
task, iterates frames, waits for render convergence, and writes the output
images.

This strategy is used when the scene does not contain USD render settings
prims and the render delegate does not request ownership of the render pass.

## Hydra V1 Render Settings

In this mode the render configuration comes from **USD render settings
prims** authored inside a USD stage in the Maya scene (typically within a
MayaUsdProxyShape):

- **UsdRenderSettings** — top-level render configuration (resolution, camera
  relationship, per-delegate settings).
- **UsdRenderProduct** — describes a single output image (output path,
  resolution override, camera override).
- **UsdRenderVar** — defines an individual AOV within a render product
  (name, data type, source).

The batch renderer reads these prims from the USD stage, extracts the
relevant parameters (resolution, camera, AOVs, render products), and applies
them to the Hydra task controller.  As with the Maya strategy, the batch
renderer still manages the render loop, convergence detection, and image
output — but the source of truth for the configuration is the USD stage
rather than Maya globals.

Each render product is processed individually.  Products may override the
resolution and camera defined in the parent UsdRenderSettings prim, and each
product's render vars determine which AOVs are rendered.

This strategy is selected automatically when USD render settings prims are
present in the scene and the render delegate does not request ownership of
the render pass.

## Hydra V2 Render Settings

In this mode the **render delegate itself** owns the render settings logic.
The render delegate reads USD render settings prims directly from the Hydra
scene and drives the render pass internally, including configuration, render
loop management, convergence, and image output.

The batch renderer only provides the execution environment: it creates the
Hydra render index, registers scene indices, and calls into the Hydra engine,
but it does not interpret render settings or manage convergence itself.

This strategy is currently used by Hydra PRMan when the environment variable
`HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS` is set to `true`.  Other render
delegates may adopt this approach in the future.

## Command-line Flags

When running a batch render through the standard Maya `Render` command-line
tool with `-renderer HdStormRendererPlugin`, `-renderer HdArnoldRendererPlugin`
or `-renderer HdPrmanLoaderRendererPlugin`, the following Maya Hydra-specific
flags are honored in addition to the renderer-agnostic Maya flags:

| Flag | Parameters | Description |
|------|-----------|-------------|
| `-x` | `int` | Set the X resolution of the final image. |
| `-y` | `int` | Set the Y resolution of the final image. |
| `-reg` | `int int int int` | Set a crop region in pixel coordinates: `left right bottom top` (Y-up, origin bottom-left, **inclusive on all four sides**). The output image keeps the full `-x`/`-y` resolution; pixels outside the region are filled with the AOV's clear/black value. This matches the Maya Software renderer's `-reg` semantics. |
| `-rd` | `path` | Directory in which to store image files. |
| `-im` | `filename` | Image file output name. |
| `-of` | `format` | Output image file format. |

Internally `-reg` writes `UsdRenderProduct.dataWindowNDC` on every render
product picked by `mayaHydra.renderSettings.renderProducts.getRenderProductsToApplySettings()`,
so it applies uniformly to the Hydra V1 and Hydra V2 render-settings paths
described above.  Pixel coordinates are converted to USD's normalized
aperture coordinates [0, 1] using:

```
xmin =  left          / W
ymin =  bottom        / H
xmax = (right  + 1)   / W   # +1 because right is INCLUSIVE
ymax = (top    + 1)   / H   # +1 because top   is INCLUSIVE
```

So `-reg 0 W-1 0 H-1` covers the full image, `-reg p p p p` selects exactly
one pixel at `(p, p)`, and a region with `right < left` or `top < bottom`
raises a Python `RuntimeError`.

## Strategy Selection

The render settings strategy is determined at render time by
`ReadRenderSettingsTypeFromRenderDelegate()` using the following logic:

1. If the render delegate signals that it drives the render pass (e.g.
   PRMan with `HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS` enabled),
   **Hydra V2** is selected.
2. Otherwise, if USD render settings prims are found in any
   MayaUsdProxyShape stage in the scene, **Hydra V1** is selected.
3. If neither condition is met, the **Maya** strategy is used.

## Related Source Files

| File | Description |
|------|-------------|
| `renderSettingsUtils.h / .cpp` | `RenderSettingsType` enum and strategy selection logic |
| `batchRenderer.h / .cpp` | Core batch renderer (shared infrastructure) |
| `batchRendererMayaRenderSettings.h / .cpp` | Maya render settings strategy |
| `batchRendererHydraV1RenderSettings.h / .cpp` | Hydra V1 render settings strategy |
| `batchRendererHydraV2RenderSettings.h / .cpp` | Hydra V2 render settings strategy |
| `hydraRenderCmd.h / .cpp` | `hydraRender` command entry point |
| `hydraRenderCmdMayaRenderSettings.cpp` | Command-level logic for Maya strategy |
| `hydraRenderCmdHydraV1RenderSettings.cpp` | Command-level logic for Hydra V1 strategy |
| `hydraRenderCmdHydraV2RenderSettings.cpp` | Command-level logic for Hydra V2 strategy |
