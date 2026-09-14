<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Batch Render Settings

MayaHydra supports batch rendering through the `hydraRender` command.  When a
batch render is initiated, MayaHydra must decide how to obtain the render
configuration — resolution, camera, output paths, AOVs (arbitrary output
variables), and other render settings.  Two strategies are available, and the
appropriate one is selected automatically based on the render delegate in use
and the contents of the scene.

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
them to the Hydra task controller.  The batch renderer manages the render
loop, convergence detection, and image output — and the source of truth for
the configuration is the USD stage.

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
tool with a Hydra renderer (e.g. `-renderer HdStormRendererPlugin`), the
following Maya Hydra-specific flags are honored in addition to the
renderer-agnostic Maya flags.  These flags are wired in by the per-renderer
description XMLs under `renderDesc/`, so any Hydra render delegate whose
description includes the corresponding `<mel>` entries will honor them.

| Flag | Parameters | Description |
|------|-----------|-------------|
| `-x` | `int` | Set the X resolution of the final image. |
| `-y` | `int` | Set the Y resolution of the final image. |
| `-reg` | `int int int int` | Set a crop region in pixel coordinates: `left right bottom top` (Y-up, origin bottom-left, **inclusive on all four sides**). The output image keeps the full `-x`/`-y` resolution; pixels outside the region are filled with the AOV's clear/black value. This matches the Maya Software renderer's `-reg` semantics. |
| `-s` | `float` | Starting frame for an animation sequence. |
| `-e` | `float` | End frame for an animation sequence. |
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

## Renderer Selection

Before any render settings strategy is chosen, `hydraRender` must first
resolve **which render delegate** to use.  Resolution follows a two-step
precedence:

1. **Explicit `-renderer`/`-r` flag.**  If the flag is set, its value is
   used directly as the Hydra render delegate's plugin id.  An explicitly
   empty flag value (`-renderer ""`) is a hard error.
2. **The `currentRenderer` attribute on the `UsdDefaultRenderDescription`
   singleton node.**  If no flag is given, this USD-authored string
   attribute is read and, if non-empty, used as the render delegate's
   plugin id.

`defaultRenderGlobals.currentRenderer` (the classic Maya Render Settings
renderer, e.g. `"arnold"` for MtoA's legacy renderer) is never read by this
resolution logic — it is a separate, unrelated attribute consulted only by
the legacy (non-Hydra) `render`/`Render` command path.

### Version-contract requirement

The `UsdDefaultRenderDescription` node and its `currentRenderer` attribute
are looked up unconditionally, without feature-detecting whether they
exist.  In every supported production configuration, MayaUSD's
`UsdDefaultRenderDescription` singleton (and its `currentRenderer`
attribute) is always present, so a missing node or attribute is treated as
a coding error, not as "the feature is disabled".  Either that case or an
unauthored (empty) attribute value is reported as "no renderer specified".

### Error posture

Resolution failures always produce a clear error and abort the batch
render — there is no silent fallback to a default renderer:

- **No flag and an empty/missing `currentRenderer`:** fails with "no
  renderer specified..." before any render delegate is created.
- **An unrecognized renderer name** (from either the flag or the
  attribute): resolution itself does not validate the name against Maya's
  registered renderers.  Instead, the name is looked up against the
  registered Hydra render delegates when the batch renderer initializes;
  if no matching render delegate plugin is found, it fails with "unknown
  or unregistered renderer...".  Either way, the failure surfaces as a
  Python `RuntimeError` from `cmds.hydraRender()` and does not crash or
  otherwise disturb the Maya session — a subsequent `hydraRender` call
  with a valid renderer succeeds normally.

### No hardcoded default renderer

This is intentional: `hydraRender` never falls back to a hardcoded renderer
(e.g. Storm) on its own.  Callers must either pass `-renderer`/`-r`
explicitly or author the `currentRenderer` attribute (see "No flag and an
empty/missing `currentRenderer`" above).

## Strategy Selection

The render settings strategy is determined at render time by
`ReadRenderSettingsTypeFromRenderDelegate()` using the following logic:

1. If the render delegate signals that it drives the render pass (e.g.
   PRMan with `HD_PRMAN_RENDER_SETTINGS_DRIVE_RENDER_PASS` enabled),
   **Hydra V2** is selected.
2. Otherwise, if USD render settings prims are found in any
   MayaUsdProxyShape stage in the scene, **Hydra V1** is selected.
3. If neither condition is met, batch rendering fails with an error.

## Related Source Files

| File | Description |
|------|-------------|
| `renderSettingsUtils.h / .cpp` | `RenderSettingsType` enum and strategy selection logic; reading the USD `currentRenderer` attribute |
| `batchRenderer.h / .cpp` | Core batch renderer (shared infrastructure); validates the selected renderer against the registered Hydra render delegates |
| `batchRendererHydraV1RenderSettings.h / .cpp` | Hydra V1 render settings strategy |
| `batchRendererHydraV2RenderSettings.h / .cpp` | Hydra V2 render settings strategy |
| `hydraRenderCmd.h / .cpp` | `hydraRender` command entry point; resolves which renderer to use (`-renderer`/`-r` flag, then `currentRenderer`) |
| `hydraRenderCmdHydraV1RenderSettings.cpp` | Command-level logic for Hydra V1 strategy |
| `hydraRenderCmdHydraV2RenderSettings.cpp` | Command-level logic for Hydra V2 strategy |
