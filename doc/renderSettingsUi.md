# Render settings UI layout

The render settings option box and the Hydra settings in the Attribute Editor (on
`defaultRenderGlobals`) group each renderer's settings into collapsible categories. The
categories, their order, the order of settings inside them and the labels come from JSON
layout files. They are read when the mayaHydra plugin loads, so changing a layout only needs a
plugin reload or a Maya restart, not a rebuild.

## Where layout files are found

1. Each folder in the `MAYAHYDRA_RENDER_SETTINGS_UI_PATH` environment variable, in order. Separate
   folders with `:` on macOS and Linux and `;` on Windows.
2. The default folder `<install>/lib/mayaHydra/renderSettingsUi`, which holds `flash.json` and
   `arnold.json`. Their sources are in `lib/mayaHydra/mayaPlugin/renderSettingsUi`. Storm has no
   default layout on purpose, so it shows what a renderer without a layout file looks like.

Every `*.json` file in a folder is read. The `renderer` key says which renderer a file is for;
the filename doesn't matter. The first file found for a renderer is used, and it replaces that
renderer's default layout as a whole. Layouts are not merged.

A renderer with no layout file, like Storm, shows its settings as one flat list, in the order
the render delegate reports them.

## Format

```json
{
  "renderer": "HdStormRendererPlugin",
  "categories": [
    {
      "label": "Lighting and shadows",
      "collapsed": false,
      "settings": [
        {"key": "maxLights", "label": "Max lights"},
        "domeLightCubemapTargetMemory"
      ]
    }
  ],
  "other": {"label": "Other", "collapsed": true},
  "hidden": ["enableTinyPrimCulling"]
}
```

| Key | Required | Meaning |
|---|---|---|
| `renderer` | yes | The render delegate plugin id, as listed by `mayaHydra -listRenderers`. |
| `categories` | no | Groups, shown in file order. |
| `categories[].label` | yes | Group title. Use sentence case. |
| `categories[].collapsed` | no | Whether the group starts collapsed. Defaults to `false`. |
| `categories[].settings` | no | Settings in display order. A bare key uses the render delegate's display name as the label. An object can set its own `label`. |
| `other` | no | The group for settings that no category lists, in delegate order. Defaults to `{"label": "Other", "collapsed": false}`. If a category has the same label, the leftovers are appended to it. |
| `hidden` | no | Keys that are never shown. |

Keys are the render delegate's setting ids, such as `maxLights` or `hdFlash:gtao`. Three
MayaHydra attributes can be placed the same way: `mtohMaximumShadowMapResolution` (Storm),
`mayaHydraSelectionHighlightMode` (Storm and Flash) and `mayaHydraOutlineHoverHighlighting`
(Storm, on builds that support hover). Whether one of them appears for a renderer is still
decided by MayaHydra. Listing it only places it.

A key the renderer doesn't report is skipped, so one file can serve several versions of a
render delegate. A key listed in several categories shows in the first one.

Refine level and the display purpose settings apply to all renderers. They always show in the
"All renderers" group and can't be placed by a layout file.

## Troubleshooting

A file that isn't valid JSON or doesn't match the format is skipped with a warning naming the
file and the problem. The next folder in the search path, or the flat list, is used instead.
Set `TF_DEBUG=MAYAHYDRAPLUGIN_RENDER_SETTINGS_UI` to print which file each renderer uses.
