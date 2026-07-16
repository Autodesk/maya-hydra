# Render delegate guide: topology change vs deformation vs full mesh rebuild

This document is for **render delegate authors** consuming MayaHydra scene-index dirty notices for **Maya-native geometry**. It explains how to tell when a mesh needs a **full rebuild** (topology or connectivity changed) versus a **deformation-only update** (points/normals moved but connectivity is stable), including skinning, blend shapes, and other deformers.

---

## Two Maya-native mesh paths

MayaHydra translates **Maya mesh data** through one of two adapters. **Emission policy differs** between them; the render-delegate contract (topology locators → rebuild) does not.

**Scope:** This section applies only to Maya-native meshes. USD meshes are translated to Hydra via **USD imaging** (`UsdImaging`), not these adapters. Custom data providers use their own dirtying rules.

| Adapter | When used |
|---------|-----------|
| [`MayaHydraRenderItemAdapter`](../lib/mayaHydra/hydraExtensions/adapters/renderItemAdapter.cpp) | **Default interactive viewport** — reads `MRenderItem` vertex/index buffers |
| [`MayaHydraMeshAdapter`](../lib/mayaHydra/hydraExtensions/adapters/meshAdapter.cpp) | Batch/production rendering; interactive viewport when `MAYA_HYDRA_USE_MESH_ADAPTER=1` |

Integration tests mirror this split: `MeshDirtyLocators` (mesh adapter) vs `RenderItemDirtyLocators` (render items).

---

## Summary

| Situation | Render delegate action |
|-----------|-------------------------|
| **Topology / connectivity change** | **Full mesh rebuild** — reallocate buffers, rebuild index buffers, refresh all primvars |
| **Deformation only** (skin, blend shape, cluster, vertex move with stable connectivity) | **Deformation path** — update dirtied buffers in place; keep topology |
| **Granular primvar edit** (UVs, tangents, etc.) | Update only the dirtied primvar channels unless topology locators are also present |
| **Transform only** | Update transform; no mesh rebuild |

The signal for “cheap vs expensive” is **whether topology changed**, not whether dirties came from `primvars/points` vs `extComputationPrimvars/points`.

Hydra treats dirty points and other dirty primvars as **deformation-only when topology is stable**, regardless of whether points are supplied via a computation or a direct buffer.

**For render delegates:** interpret locators the same way on both paths — `mesh/topology`, `mesh/subdivisionScheme`, or broad `primvars` (parent locator) ⇒ rebuild; granular child locators only ⇒ update those channels.

---

## Hydra 2.0 locators (what maya-hydra emits)

MayaHydra uses [`DirtyNotifier`](../lib/flowViewport/fvpDirtyNotifier.h) to emit `HdDataSourceLocator`s directly (Hydra 2.0 / scene index path).

### Topology change (full rebuild)

When Maya connectivity changes (extrude, merge, poly smooth level crossing, render-item topo delta, etc.), maya-hydra emits topology-related locators. **`extComputationPrimvars` is not emitted on topology changes.**

#### Mesh adapter (`DirtyRprimConnectivityLocators` with `HdPrimTypeTokens->mesh`)

Used for `MPolyMessage` topology / component-id callbacks on meshes:

| Concept | Locators (tokens) |
|---------|-------------------|
| Topology | `mesh/subdivisionScheme`, `mesh/topology` |
| All face-varying primvars | broad `primvars` |
| Points | `primvars/points` |
| Bounds | `extent` |

Granular `primvars/st`, `primvars/tangents`, and `primvars/normals` are **not** emitted on the topology path — broad `primvars` plus topology locators are sufficient for a full rebuild.

#### NURBS curve adapter (`DirtyRprimConnectivityLocators` with `HdPrimTypeTokens->basisCurves`)

Used for `MPolyMessage` topology / component-id callbacks on NURBS curves (`basisCurves` prims):

| Concept | Locators (tokens) |
|---------|-------------------|
| Topology | `basisCurves/topology` |
| All face-varying primvars | broad `primvars` |
| Points | `primvars/points` |
| Bounds | `extent` |

#### Render item adapter (`_EmitRenderItemTopologyDirtyLocators`)

Used when `RenderItemShouldEmitTopologyLocators` returns true (see [topology suppression](#render-item-topology-suppression) below). Emits **topology locators only** — no granular UV/tangent/normal locators and no broad `primvars`. When Maya also sets `geomChanged` alongside `topoChanged`, vertex-buffer re-read dirties `primvars/points` (and possibly `primvars/st` / `primvars/tangents`) via the separate `geomChanged` path.

**Mesh** (`kTriangles`, `kTriangleStrip`):

| Concept | Locators (tokens) |
|---------|-------------------|
| Topology | `mesh/subdivisionScheme`, `mesh/topology` |
| Points | `primvars/points` (only when `geomChanged` is also true, e.g. vertex count change) |
| Bounds | `extent` (only when the bounding box actually changed) |

**Curves** (`kLines`, `kLineStrip` → `basisCurves` prims):

| Concept | Locators (tokens) |
|---------|-------------------|
| Topology | `basisCurves/topology` |
| Points | `primvars/points` (only when `geomChanged` is also true) |
| Bounds | `extent` (only when the bounding box actually changed) |

**Rule for render delegate:** if **any** of `mesh/topology`, `mesh/subdivisionScheme`, `basisCurves/topology`, or broad `primvars` (parent locator, not only a child) appear in the dirty set for an rprim, assume connectivity or face-varying layout may have changed → **full rebuild** (or at least re-pull topology and all primvars).

### Geometry / deformation change (no topology)

When only vertex positions (or normals) change and connectivity is unchanged:

| Concept | Locators (tokens) |
|---------|-------------------|
| Points | `primvars/points` |
| Normals | `primvars/normals` (when maya-hydra passes normals to Hydra) |
| Bounds | `extent` (mesh adapter: always on point edits; render item: only when bbox changed) |
| Specific primvars | `primvars/<name>` e.g. `primvars/st`, `primvars/tangents` |

#### Path-specific deformation emission

| Event | Mesh adapter | Render item adapter |
|-------|--------------|---------------------|
| Vertex move / skinning | `primvars/points`, `extent`, `subdivisionTags` (+ `primvars/normals` if enabled) | `primvars/points`, `primvars/st`, `primvars/tangents` (+ `primvars/normals` if enabled); `extent` only if bbox changed |
| UV-only edit | `primvars/st` | `primvars/st` (+ `primvars/points` when Maya re-reads all vertex buffers via `geomChanged`) |

**Rule for render delegate:** if the dirty set contains **only** point/normal/extent/subdivisionTags (and/or specific primvar children) and **does not** include topology locators or broad `primvars`, treat as **deformation-only** → update buffers without rebuilding index topology.

This applies to:

- Skeletal skinning and blend shapes (today maya-hydra dirties `primvars/points` / `primvars/normals`)
- Clusters and other non-skeleton deformers
- Interactive component moves (render item may also dirty `primvars/st` / `primvars/tangents` because vertex buffers are re-read as a batch)
- UsdSkel / extComputation paths (see below)

### Render item topology suppression

Maya may set `MVS_changedTopo` alongside `MVS_changedGeometry` for operations that do not change connectivity (e.g. moving a vertex). The render item adapter suppresses topology locators when **both** vertex count and index connectivity are unchanged ([`RenderItemShouldEmitTopologyLocators`](../lib/mayaHydra/hydraExtensions/adapters/renderItemTopologyUtil.cpp)).

When connectivity changes with the same vertex count (e.g. edge flip), topology locators **are** emitted. Render delegates should not treat `primvars/points` alone as topology-stable if `mesh/topology` is also in the same notice.

---

## `primvars/points` vs `extComputationPrimvars/points`

These locators describe **how Hydra obtains** point data, not how expensive the **consumer** update should be:

| Locator | Meaning |
|---------|---------|
| `primvars/points` | Resolved points buffer (read from primvars schema) |
| `extComputationPrimvars/points` | Points come from a computation primvar (Hydra or a scene index runs the computation; Storm may use GPU skinning) |

**Today in maya-hydra:** deformed meshes are still served through **primvars**; we dirty `primvars/points` during animation. We do **not** yet insert computation prims for Maya skinning/blend shapes.

**For your render delegate:**

- Do **not** infer “full rebuild” from `primvars/points` alone.
- Do **not** infer “deformation only” from `extComputationPrimvars/points` alone.
- **Do** use **topology locators absent + points (or extComp points) dirty** → deformation path.

Reference implementations (OpenUSD):

- `UsdSkelImaging` — skinning via extComputation primvars
- `HdSiExtComputationPrimvarPruningSceneIndex` — resolving computation-backed primvars before the renderer

---

## Pixar guidance (March 2026)

Render delegates consuming MayaHydra **must not** treat `primvars/points` alone as a full-rebuild signal when topology locators are absent. MayaHydra emission policy aligned with this guidance:
   - **Topology changes** → topology locators only (plus broad `primvars` on the mesh-adapter connectivity path). Do **not** also dirty granular UV/tangent/normal locators — the render delegate should full-rebuild from topology.
   - **Deformation** (topology stable) → granular `primvars/points` / `primvars/normals`

**Note:** On the render item adapter, `geomChanged` re-reads all vertex buffers as a batch, so deformation may also dirty `primvars/st` and `primvars/tangents` alongside points (and normals when `useMayaNormals` is enabled). When Maya sets `geomChanged` alongside a real topology change (e.g. extrude), those granular locators appear in the same notice but originate from the deformation re-read path, not the topology helper — render delegates should still use topology locators (or broad `primvars`) to decide full rebuild vs in-place update.

---

## Recommended render delegate logic

Pseudocode for processing `HdSceneIndexObserver::PrimsDirtied` entries:

```cpp
void OnPrimDirtied(SdfPath const& id, HdDataSourceLocatorSet const& locators)
{
    const bool topologyDirty =
        locators.Intersects(HdMeshTopologySchema::GetDefaultLocator()) ||
        locators.Intersects(HdMeshSchema::GetSubdivisionSchemeLocator()) ||
        locators.Intersects(HdBasisCurvesTopologySchema::GetDefaultLocator()) ||
        locators.Intersects(HdPrimvarsSchema::GetDefaultLocator()); // broad primvars parent

    const bool pointsDirty =
        locators.Intersects(HdPrimvarsSchema::GetPointsLocator());

    const bool normalsDirty =
        locators.Intersects(HdPrimvarsSchema::GetNormalsLocator());

    if (topologyDirty) {
        RebuildMeshFromSceneIndex(id);  // full pull: topology + all primvars + extent
        return;
    }

    if (pointsDirty || normalsDirty) {
        UpdateDeformedBuffers(id, pointsDirty, normalsDirty);  // in-place update
        if (locators.Intersects(HdExtentSchema::GetDefaultLocator())) {
            UpdateBounds(id);
        }
        return;
    }

    // Handle other locators: xform, materialBindings, primvars/st, visibility, ...
}
```

### Caching topology generation

Keep a **topology generation counter** (or hash of face counts + index buffer) per rprim:

1. On dirty, if topology locators present → increment generation, full rebuild.
2. If only `primvars/points` (and/or other granular primvars) → if generation unchanged, deformation update only.

This matches the distinction between deforming geometry and stable topology.

### Edge cases

| Case | Guidance |
|------|----------|
| First sync / new prim | Full pull (no cached topology) |
| Broad `primvars` + `primvars/points` in same notice | Full rebuild (topology path may have fired in same flush) |
| `mesh/topology` without broad `primvars` (render item path) | Full rebuild — topology locators alone are sufficient |
| Only `extent` | Update bounds only |
| Smooth mesh toggle | Mesh adapter: `displayStyle` + topology + `subdivisionTags`. Render item: topology only; `geomChanged` may add granular primvars |
| `subdivisionTags` without topology | Mesh adapter may emit on `inMesh`/`pnts` edits; re-pull subdivision tags |
| Extension/dynamic attribute add/set/remove | Broad `primvars` + `extComputationPrimvars` (not a topology change) |
| Instancing | Also watch `instancedBy`, `instancerTopology` for instance count/transform changes |

---

## What maya-hydra sends during skinning / blend shapes (today)

During animated deformation (Evaluation Manager / DG deforming the mesh):

- **Dirty:** `primvars/points`, often `primvars/normals`, sometimes `extent` (render item: extent only if bbox changed; render item may also dirty `primvars/st` / `primvars/tangents` when vertex buffers are re-read)
- **Not dirty:** `mesh/topology`, `mesh/subdivisionScheme`, broad `primvars` (unless a separate topology edit occurred)

So a render delegate that rebuilds the mesh on every `primvars/points` dirty will be **correct but slow**. Implement the topology-stable fast path above.

---

## MayaHydra dirtying policy (for context)

### Mesh adapter (`MayaHydraMeshAdapter`)

| Event | Emission pattern |
|-------|------------------|
| Poly topology / component id change (`MPolyMessage` callbacks) | Topology + broad `primvars` + `primvars/points` + `extent` |
| `inMesh` / `pnts` dirty — UV-only (connectivity + points unchanged) | Granular `primvars/st` only |
| `inMesh` / `pnts` dirty — geometry changed | `primvars/points` + `extent` + `subdivisionTags` (+ `primvars/normals` if enabled); **no** topology locators (topology edits rely on `MPolyMessage` callbacks above) |
| UV set change (`MPolyMessage::addUVSetChangedCallback`) | Granular `primvars/st` |
| `uvPivot` attribute change | Granular `primvars/st` |
| Smooth mesh toggle (`displaySmoothMesh` / `smoothLevel`) | `displayStyle` + topology + `subdivisionTags`; **no** broad `primvars`, **no** granular UV/tangent/normal on topology path |
| Extension/dynamic attribute add/set/remove | Broad `primvars` + `extComputationPrimvars` |
| Transform | `xform` only |

### Render item adapter (`MayaHydraRenderItemAdapter`)

| Event | Emission pattern |
|-------|------------------|
| Connectivity change (`topoChanged` + `RenderItemShouldEmitTopologyLocators`) | Topology locators only (`mesh/topology` or `basisCurves/topology`); **no** broad `primvars`, **no** granular UV/tangent/normal on topology path, **no** `extComputationPrimvars` |
| Vertex move / deformation (`geomChanged`, connectivity unchanged) | `primvars/points` + `primvars/st` + `primvars/tangents` (+ `primvars/normals` if enabled); topology locators suppressed |
| Extent | `extent` only when bounding box actually changed (diff against stored bounds) |
| Smooth mesh toggle | Topology only; **no** `displayStyle`; `geomChanged` may add granular primvars when Maya re-reads buffers |
| Extension/dynamic attribute add/set/remove | Broad `primvars` + `extComputationPrimvars` |
| Transform | `xform` only |

Implementation: [`meshAdapter.cpp`](../lib/mayaHydra/hydraExtensions/adapters/meshAdapter.cpp), [`renderItemAdapter.cpp`](../lib/mayaHydra/hydraExtensions/adapters/renderItemAdapter.cpp), [`renderItemTopologyUtil.cpp`](../lib/mayaHydra/hydraExtensions/adapters/renderItemTopologyUtil.cpp), [`fvpDirtyNotifier.cpp`](../lib/flowViewport/fvpDirtyNotifier.cpp), [`adapter.cpp`](../lib/mayaHydra/hydraExtensions/adapters/adapter.cpp) (extension/dynamic primvars).

Unit tests for locator mapping: [`testDirtyNotifier.cpp`](../test/lib/mayaUsd/render/mayaToHydra/cpp/testDirtyNotifier.cpp).

Integration tests (interactive Maya): [`testDirtyLocators.cpp`](../test/lib/mayaUsd/render/mayaToHydra/cpp/testDirtyLocators.cpp) — `MeshDirtyLocators` suite via [`testMeshDirtyLocators.py`](../test/lib/mayaUsd/render/mayaToHydra/cpp/testMeshDirtyLocators.py) (mesh adapter mode), `RenderItemDirtyLocators` suite via [`testRenderItemDirtyLocators.py`](../test/lib/mayaUsd/render/mayaToHydra/cpp/testRenderItemDirtyLocators.py) (render items mode).

---

## References

- Deformation-only updates when topology is stable; UsdSkel uses extComputation primvars for GPU skinning; resolved CPU buffers via primvars are also valid.
- OpenUSD: `pxr/imaging/hd/dirtyBitsTranslator.cpp` — legacy dirty bit → locator mapping
- OpenUSD: `UsdSkelImaging`, `HdSiExtComputationPrimvarPruningSceneIndex`
