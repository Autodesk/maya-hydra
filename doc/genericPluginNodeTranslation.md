# Generic Plugin Node Translation to Hydra

## Overview

Maya-hydra provides a generic translation mechanism that allows custom DAG nodes created by third-party Maya plugins (Arnold, RenderMan, custom in-house plugins, etc.) to participate in the Hydra scene without maya-hydra needing any knowledge of the plugin's node types.

This is useful for plugin-defined node types such as `aiPhotometricLight`, `aiLightBlocker`, `PxrDomeLight`, or any custom node that has no dedicated adapter registered in `MayaHydraAdapterRegistry`.

## Architecture

```
 Maya DAG Node (plugin-defined)
        |
        v
 MayaHydraSceneIndex::InsertDag()
        |
        |  1. Try light adapter   -> no match
        |  2. Try camera adapter  -> no match
        |  3. Try shape adapter   -> no match
        |  4. Is it a plugin node? (MNodeClass::pluginName() non-empty?)
        |     - No  -> skip (Maya built-in node)
        |     - Yes -> does it already provide its own scene index?
        |              (e.g. mayaUsdProxyShape)
        |       - Yes -> skip (already in Hydra via its own scene index)
        |       - No  -> create MayaHydraGenericDagAdapter
        |
        v
 Hydra Scene Index
 (prim type: "mayaCustomDagNode")
 (data sources: xform, visibility, purpose, mayaNode)
        |
        v
 Render Delegate's HdSceneIndexPlugin
 (reads mayaNode -> re-types prim, maps attributes)
        |
        v
 Render Delegate (Arnold, PRMan, Storm, etc.)
```

## How It Works

### Maya-Hydra Side

1. **Detection**: When `MayaHydraSceneIndex::InsertDag()` encounters a DAG leaf node that no registered adapter claims, it checks if the node was registered by a plugin using `MNodeClass(typeName).pluginName()`. Only plugin-registered nodes are translated; Maya built-in nodes (locators, joints, constraints, etc.) are always skipped. Plugin nodes that already provide their own Hydra scene index (e.g., `mayaUsdProxyShape`) are also excluded to avoid duplicate prims.

2. **Adapter creation**: A `MayaHydraGenericDagAdapter` is created. This adapter:
   - Caches the Maya node type name (e.g., `"aiPhotometricLight"`).
   - Reads all non-default attribute values into a `VtDictionary`.
   - Makes no assumption about what the node represents (light, mesh, camera, etc.).

3. **Prim insertion**: The adapter inserts a Hydra prim with type `mayaCustomDagNode` into the scene index. The prim carries these data sources:
   - `xform` -- standard Hydra transform from the DAG path.
   - `visibility` -- standard Hydra visibility from the DAG path.
   - `purpose` -- standard Hydra purpose.
   - `mayaNode` -- a custom container data source (`MayaHydraGenericNodeDataSource`) with:
     - `mayaTypeName` (`TfToken`): the Maya node type name.
     - `mayaAttributes` (`VtDictionary`): all non-default attribute name/value pairs.

4. **Attribute filtering**: Standard Maya built-in base class attributes (from `locator`, `shape`, `dagNode`, `dependNode`, etc.) are excluded since maya-hydra already handles them through xform, visibility, and purpose data sources. This covers attributes like `castsShadows`, `receiveShadows`, `worldPosition`, `visibility`, `objectColorRGB`, etc. Message attributes are also excluded. Among the remaining plugin-specific attributes, only those whose value differs from their registered default are included in `mayaAttributes`. If a plugin node currently has no qualifying attributes, the `mayaAttributes` dictionary is empty but the prim is still created -- this ensures that when the user later modifies an attribute, the existing attribute-changed callback fires a dirty notice and the data source returns the updated value.

5. **Dirty notifications**: When a Maya attribute changes on the node, the adapter fires a Hydra dirty notice with a per-attribute locator: `mayaNode.mayaAttributes.<attrName>`. This allows downstream scene indices to react to specific attribute changes efficiently. Transform and visibility changes use the standard Hydra locators.

### Render Delegate Side

The render delegate provides its own `HdSceneIndexPlugin` (registered via `HdSceneIndexPluginRegistry::RegisterSceneIndexForRenderer`) that:

1. **Observes** incoming prims for the `mayaCustomDagNode` type.
2. **Reads** the `mayaNode` data source to get the Maya type name and attributes.
3. **Translates** the prim by re-typing it and mapping Maya attributes to the Hydra schema the render delegate expects (e.g., UsdLux tokens for lights).
4. **Ignores** Maya type names it does not recognize (passes them through unchanged).

**The render delegate has zero dependency on maya-hydra.** It only depends on standard OpenUSD APIs. The token names (`mayaCustomDagNode`, `mayaNode`, `mayaTypeName`, `mayaAttributes`) are a documented convention -- the render delegate uses them as string literals:

```cpp
static const TfToken kMayaCustomDagNode("mayaCustomDagNode");
static const TfToken kMayaNode("mayaNode");
static const TfToken kMayaTypeName("mayaTypeName");
static const TfToken kMayaAttributes("mayaAttributes");
```

## Convention Tokens

| Token | Type | Description |
|---|---|---|
| `mayaCustomDagNode` | Prim type | The Hydra prim type used for all generic plugin nodes. |
| `mayaNode` | Data source key | Top-level container on the prim holding the generic node data. |
| `mayaTypeName` | Data source key | Child of `mayaNode`. `TfToken` with the Maya node type name. |
| `mayaAttributes` | Data source key | Child of `mayaNode`. `VtDictionary` with non-default attribute values. |

These tokens are defined in `lib/mayaHydra/hydraExtensions/adapters/tokens.h`.

## Dirty Locators

When a Maya attribute changes, the dirty locator is:

```
mayaNode.mayaAttributes.<attrName>
```

For example, changing `intensity` produces the locator `mayaNode.mayaAttributes.intensity`.

A render delegate's scene index can check for specific attributes:

```cpp
// React only to intensity changes
HdDataSourceLocator intensityLoc(kMayaNode, kMayaAttributes, TfToken("intensity"));
if (entry.dirtyLocators.Intersects(intensityLoc)) {
    // re-read intensity only
}

// React to any attribute change (parent locator intersects all children)
HdDataSourceLocator anyAttrLoc(kMayaNode, kMayaAttributes);
if (entry.dirtyLocators.Intersects(anyAttrLoc)) {
    // re-read all attributes
}
```

## Example: Arnold aiPhotometricLight

Arnold is used here as a concrete illustration, but the same pattern applies to **any render delegate** (RenderMan, custom engines, etc.). Each render delegate would provide its own scene index plugin registered for its renderer name, handling the Maya node types it cares about and passing through the rest.

A complete example is provided in `lib/mayaHydra/flowViewportAPIExamples/genericNodeTranslationSceneIndex/`. It demonstrates how a render delegate's scene index would translate an `aiPhotometricLight` into a `sphereLight` with UsdLux-compatible attributes.

### What maya-hydra produces

When an `aiPhotometricLight` node with `intensity=2.5`, `aiExposure=3.0`, and `aiFilename="/path/to/light.ies"` is in the scene:

- **Prim type**: `mayaCustomDagNode`
- **`mayaNode.mayaTypeName`**: `"aiPhotometricLight"`
- **`mayaNode.mayaAttributes`**: `{ "intensity": 2.5, "aiExposure": 3.0, "aiFilename": "/path/to/light.ies" }` (plus any other non-default attributes)

### What the example scene index does

The `HdGenericNodeTranslationSceneIndex` intercepts this prim in `GetPrim()`, recognizes `"aiPhotometricLight"`, and returns:

- **Prim type**: `sphereLight`
- **Data sources**: UsdLux-compatible tokens (`inputs:intensity`, `inputs:exposure`, `inputs:shaping:ies:file`, etc.) overlaid on the original xform/visibility.

The Arnold render delegate then detects the `inputs:shaping:ies:file` attribute and automatically creates an Arnold `photometric_light` node.

### Attribute mapping

| Maya attribute | UsdLux token | Arnold parameter |
|---|---|---|
| `intensity` | `inputs:intensity` | `intensity` |
| `color` | `inputs:color` | `color` |
| `aiExposure` | `inputs:exposure` | `exposure` |
| `aiFilename` | `inputs:shaping:ies:file` | `filename` |
| `aiRadius` | `inputs:radius` | `radius` |
| `aiNormalize` | `inputs:normalize` | `normalize` |
| `aiDiffuse` | `inputs:diffuse` | `diffuse` |
| `aiSpecular` | `inputs:specular` | `specular` |
| `aiCastShadows` | `inputs:shadow:enable` | `cast_shadows` |
| `aiShadowColor` | `inputs:shadow:color` | `shadow_color` |

## Extending to Other Render Delegates

Each render delegate provides its own `HdSceneIndexPlugin` that handles the Maya types it cares about. No changes to maya-hydra are required.

For example, a RenderMan integration would:

1. Create an `HdSceneIndexPlugin` registered for `"PRMan"`.
2. In `GetPrim()`, check for `mayaTypeName` values like `"PxrDomeLight"`, `"PxrRectLight"`, etc.
3. Map Maya attributes to RenderMan-specific Hydra tokens.
4. Ignore types it does not recognize.

## Extending to New Plugin Node Types

No maya-hydra changes are needed. As soon as a plugin registers a new Maya node type and creates a DAG node of that type, maya-hydra automatically translates it as a `mayaCustomDagNode`. The render delegate's scene index just needs to handle the new `mayaTypeName`.

## Files

### Core implementation

| File | Description |
|---|---|
| `lib/mayaHydra/hydraExtensions/adapters/genericDagAdapter.h/.cpp` | The generic adapter that translates plugin DAG nodes. |
| `lib/mayaHydra/hydraExtensions/sceneIndex/mayaHydraGenericNodeDataSource.h/.cpp` | Hydra data source exposing mayaTypeName and mayaAttributes. |
| `lib/mayaHydra/hydraExtensions/adapters/tokens.h` | Token definitions (mayaCustomDagNode, mayaNode, etc.). |
| `lib/mayaHydra/hydraExtensions/mixedUtils.h/.cpp` | `GetNonDefaultMayaAttributesFromNode()` utility for reading non-default attributes. |

### Scene index integration

| File | Description |
|---|---|
| `lib/mayaHydra/hydraExtensions/sceneIndex/mayaHydraSceneIndex.h/.cpp` | `CreateGenericAdapter()`, `_genericAdapters` map, `_genericsToAdd` queue. |
| `lib/mayaHydra/hydraExtensions/sceneIndex/mayaHydraDataSource.cpp` | Exposes `mayaNode` data source for `mayaCustomDagNode` prim type. |

### Example

| File | Description |
|---|---|
| `lib/mayaHydra/flowViewportAPIExamples/genericNodeTranslationSceneIndex/` | Complete example of a render delegate scene index translating aiPhotometricLight. |

### Tests

| File | Description |
|---|---|
| `test/.../cpp/testGenericDagNodeTranslation.py` | Python test: scene setup with mtoa's aiPhotometricLight. |
| `test/.../cpp/testGenericDagNodeTranslation.cpp` | C++ GTest: prim type, attributes, dirty locators, default filtering. |
