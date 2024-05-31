# Selection Highlighting Architecture

Selection highlighting changes the in-viewport appearance of selected objects
or object components.

In the following document we will refer to the software infrastructure that
supports Hydra rendering in this repository as the Flow Viewport Toolkit (name
subject to change).

This document will describe the state of Flow Viewport Toolkit selection
highlighting as of 31-May-2024.

## Behavior

Applications maintain a set of selected objects that the user can add to and
remove from.  Selected objects are usually the target of user operations, and
are shown differently in the viewport for ease of understanding.

An application will provide a way to select an object, or to select components
of an object.  For example, for a mesh object, these components may be points,
edges, or faces.  Currently, only object selection highlighting and USD point
instancing highlighting of meshes are supported.  Selection highlighting of
components is unimplemented.

## Selection: Application versus Hydra

The application maintains an edit-friendly version of the scene.  This scene is
translated into a Hydra scene by scene indices.  Correspondingly, there are two
versions of the selection, one in the application, with objects and their paths
described with application-specific classes, and a version of the selection in
the Hydra scene, described as prims and their `SdfPath`s, as well as their
associated selection data sources.

## Requirements

Requirements for selection highlighting are:

- It must be possible to provide selection highlighting in an application that
  supports multiple data models (e.g. Maya data and USD data), and plugins to
  those data models (e.g. Maya plugin nodes).

- Selected prims in the Hydra scene index tree must contain a data source
  indicating their selection state.  This data source is the one used by Pixar
  in Hydra code, and thus is used in usdview.

- Data injecting plugin scene indices must be able to specify their selection
  highlight appearance.

- It must be possible to let a data injecting data model provide prims to Hydra
  that already contain selection highlighting.  Currently,
  this is true of Maya native Dag data, where selection
  highlighting is done by OGS.

## Selection Highlighting Styles

There are at least two approaches to selection highlighting:
- **Added geometry**: adding secondary geometry that indicates the selected
status of objects in the scene, e.g. wireframe or bounding box.
- **Pixed-based modified object appearance**: rendering selected objects in a
special way, e.g. object contour, modified object color, or object overlay.

The former approach is handled by having a plugin provide a selection
highlighting filtering scene index to the Flow Viewport Toolkit, and is the
topic of this document at time of writing.  The
latter is handled by having a plugin provide a selection highlighting 
task to the Flow Viewport Toolkit, and is currently unimplemented.

## Added Geometry Plugin Software Architecture Requirements

A selection highlighting plugin that provides added geometry to scene must
provide the following services:

- A way to translate the application's selection path(s) into Hydra paths and data sources:
    - So that the appropriate prims in Hydra can be dirtied on selection change.
    - So that selected prims in Hydra can have a data source added.
  This is embodied in a **Path interface**.

- A way for the plugin to query the Hydra version of the application
  selection:
    - So that the plugin can add the appropriate selection highlighting
      geometry on those prims that require it, e.g. a different color for the
      first selected object, or selection highlighting for a complete hierarchy.

## Sample Code
### Selection Change

This
[selection change code](../lib/flowViewport/sceneIndex/fvpSelectionSceneIndex.cpp#L151-L170)
shows the use of the *Path Interface*, through the *ConvertUfeSelectionToHydra()* method,
called on the input scene index.  The path interface allows the selection scene
index to translate selected application paths to selected Hydra scene index
paths.

### Wireframe Selection Highlighting

This
[wireframe selection highlighting code](../lib/flowViewport/sceneIndex/fvpWireframeSelectionHighlightSceneIndex.cpp#L462-L465)
shows the use of the *Selection*, through the
*HasFullySelectedAncestorInclusive()* method, called on the input selection.
The selection allows a selection highlighting filtering scene index to query
selected prims.

## Design Option Discussion

- **Plugins use Hydra selection, not application selection**: selection
  highlighting plugins should only deal with the Hydra view of the selection,
  not the application view.  Selected objects should be Hydra prims, and paths
  to them described as `SdfPath`.  This keeps plugins independent of any
  particular application's representation of selection.

- **Plugins access the Hydra selection through the scene index tree**: although
  the selection is conceptually a singleton, we will provide access to it for
  scene index selection highlighting plugins through the scene index tree, by
  adding a mixin interface to the plugins.  This avoids creating another object
  to maintain and encapsulate the Hydra selection, since the selection scene
  index is already performing this job.

## Implementation

### Hydra Scene and Viewport Selection Result

The resulting wireframe selection highlighting of USD data is shown here:
![In-viewport wireframe selection highlighting](hydraSelectionHighlighting.png)

The resulting prim selection data source is shown here:
![Prim selection data source](hydraSelectionDataSource.png)

The resulting prim wireframe display style data source is shown here:
![Prim wireframe display style data source](hydraSelectionReprDisplayStyle.png)

### Flow Viewport Toolkit

The complete implementation of selection highlighting is done in a new library
in the maya-hydra repository.  The library is called `flowViewport`, under the
`lib` directory.  The library supports [semantic
versioning](https:/semver.org), with classes in a `Fvp` namespace.
The `Fvp` namespace actually contains the flowViewport major version.  All
flowViewport files have an `fvp` prefix.

The `mayaHydraLib` library and the `mayaHydra` plugin both directly depend on
`flowViewport`.

Having the selection highlighting code in a separate library promotes
reusability and enforces separation of concerns.

### Added Geometry Selection Highlighting Through Scene Indices

The Hydra scene index tree was chosen to implement selection highlighting
through additional geometry.  This is because a scene index can inject
additional prims into the scene, modify data sources of prims in the scene,
and dirty prims in the scene whose selection status has changed.

The scene index tree is now the following:
```mermaid
graph BT;
    lgcy[Scene delegate legacy]-->hm[Hydra builtin merge];
    ph1[Plugin highlighting 1]-->hm;
    subgraph ph[Plugin highlighting]
        ph2[Plugin highlighting 2]-->ph1;
        roSn[/Read-only Selection/]-.->ph1;
        roSn-.->ph2;
    end
    snSi[Selection scene index]-->ph2;
    fvpm[Flow Viewport merge]-->snSi;
    subgraph pd[Plugin data]
        p1[Plugin 1];
        p2[Plugin 2];
    end
    fvpm-. Path .->p1;
    p1-->fvpm;
    p2-->fvpm;
    snSi-. Path .->fvpm;
    sn[/Selection/]-.->snSi;
```
The plugin data and plugin highlighting subtrees are where plugins add their
scene indices.  The data scene index is required, and the highlighting scene
index is optional.

### Object Modeling

The object modeling is the following:
- **Selection**: builtin provided by the Flow Viewport Toolkit.
    - Encapsulates the Hydra selection as scene index paths and selection data sources.
    - Is shared by the selection scene index and all selection highlighting
      scene indices.
- **Selection scene index**: builtin provided by the Flow Viewport Toolkit.
    - Has a pointer to read and write the Hydra selection.
    - Translates the application selection to Hydra selection.
- **Flow Viewport merging scene index**: builtin provided by the Flow Viewport
  library.
    - Receives data from data provider plugin scene indices.
    - Forward path interface queries to plugin scene indices
- **Plugin data scene index**: provided by plugin.
    - Injects plugin data into Hydra
- **Plugin selection highlighting scene index**: provided by plugin.
    - Has a pointer to a read-only view of the Hydra selection.
    - Processes dirty selection notifications to dirty the appropriate prim(s)
      in plugin data, including hierarchical selection highlighting
    - Adds required geometry or data sources to implement selection
      highlighting

### New Scene Index Mixin Interface Base Class

The Flow Viewport Toolkit has a new mixin interface class:

- **Path Interface**: so that the builtin selection scene index can query
  plugins to translate selected object application paths to selected Hydra
  prim paths. The plugin provides the concrete implementation of this
  interface.

### Implementation Classes

- **Wireframe selection highlighting scene index**: 
    - Uses Hydra HdRepr to add wireframe representation to selected objects
      *and their descendants*.
    - Requires selected ancestor query from selection.
    - Dirties descendants on selection dirty.
- **Render index proxy**:
    - Provides encapsulated access to the builtin Flow Viewport merging scene
      index.
    - Other responsibilities to be determined, for future extension, possibly a
      [facade design pattern](https://en.wikipedia.org/wiki/Facade_pattern).

### Class Diagram

```mermaid
classDiagram
class HdSingleInputFilteringSceneIndexBase
class HdMergingSceneIndex

class Selection{
+IsFullySelected(SdfPath) bool
+HasFullySelectedAncestorInclusive(SdfPath) bool
}

class PathInterface{
+ConvertUfeSelectionToHydra(Ufe::Path) SdfPath
}

class SelectionSceneIndex
class MergingSceneIndex
class WireframeSelectionHighlightSceneIndex

class RenderIndexProxy{
+MergingSceneIndex mergingSceneIndex
+InsertSceneIndex()
+RemoveSceneIndex()
}

HdSingleInputFilteringSceneIndexBase <|-- SelectionSceneIndex

HdMergingSceneIndex <|-- MergingSceneIndex
PathInterface       <|-- MergingSceneIndex

HdSingleInputFilteringSceneIndexBase <|-- WireframeSelectionHighlightSceneIndex

RenderIndexProxy *-- MergingSceneIndex : Owns

SelectionSceneIndex ..> MergingSceneIndex : Path

SelectionSceneIndex o-- Selection : Read / Write

WireframeSelectionHighlightSceneIndex o-- Selection : Read
```

## Algorithmic Complexity

- At time of writing, for an n-element selection, membership lookup is O(log n)
  (map of SdfPath).  Ancestor membership lookup is O(n), as we loop through
  each selected path and inspect the selected path prefix.  This could be much
  improved (to amortized O(k), for a k-element path) through the use of a
  prefix trie, such as `Ufe::Trie`.

- At time of writing, merging scene index path lookup is O(n), for n input
  scene indices.  This could be improved by implementing a caching scheme based
  on application path, as for a given application path prefix the same
  input scene index will always provide the translation to scene index path.

## Limitations

- Little investigation of pixel-based selection highlighting capability.
    - Needs task-based approach.
    - Needs selection tracker object to make selection and data derived from
      the selection available to tasks through the task context data

- No selection highlighting across scene indices: selection state propagates
  down app scene hierarchy, so that when an ancestor is selected, a
  descendant's appearance may change.  This can mean selection state must
  propagate across scene index inputs, so that if a Maya Dag ancestor is
  selected, a USD descendant's appearance can change.  This is the same
  situation as global transformation and visibility.

## Mesh point instancing selection highlighting

We currently support selection highlighting for point instancing of meshes
for the three different point instancing selection modes :
- Point Instancer
- Instance
- Prototype

Here is an overview of how selection highlighting for point instancing
is implemented. We'll be using USD as the source data model, which has
some idiosyncracies of its own (which we'll go over when needed), but 
the vast majority of it is Hydra-based.

### Point instancing data sources

In Hydra, a point instancer is represented as a prim of type instancer,
with an `instancerTopology` data source. This data source contains three
inner data sources :
- The `prototypes` data source, of type `VtArray<SdfPath>`, lists the paths
to each prototype this point instancer instances.
- The `instanceIndices` data source, a vector data source where each element
data source (`i0, i1, i2, etc.`) is of type `VtArray<int>` and contains
which instances correspond to which prototype. For example, if `i1` contains
`0, 3`, then the first and fourth instances will be using the second prototype.
- The `mask` data source, of type `VtArray<bool>`, which can optionally be used
to show/hide specific instances (e.g. if the 3rd element of the mask is `false`,
then the 3rd instance will be hidden). If this array is empty, all prims will be
shown.

Per-instance data is specified using primvar data sources, namely :
- hydra:instanceTranslations
- hydra:instanceRotations
- hydra:instanceScales
- hydra:instanceTransforms
Where the corresponding primvarValue data source lists the instance-specific data.
Note that while the first three are 3-dimensional vectors and `hydra:instanceTransforms`
is a 4x4 matrix, they can all be used simultaneously (internally, they will all be
converted to 4x4 matrices, and then multiplied together).

On the other end of instancing, prototypes have an `instancedBy` data source.
This data source contains up to two inner data sources :
- (required) : The `paths` data source, of type `VtArray<SdfPath>`, lists the paths
to each instancer that instances this prototype.
- (optional) : When a sub-hierarchy is prototyped, the `prototypeRoots`, of type 
`VtArray<SdfPath>`, lists the paths to the roots of the sub-hierarchies that are being 
prototyped. For example, if we are instancing an xform that has a child mesh,
then the prototype xform and mesh prims will each have the same instancedBy data source,
where the `paths` data source will point to the instancers that use this prototype, and
where the `prototypeRoots` will point to the xform prim.

Some notes about the behavioral impacts of the hierarchical location of prims :
- Prims that are rooted under an instancer will not be drawn unless instanced
- Prototypes that are instanced will still be drawn as if they were not instanced
(i.e. the instances will be drawn in addition to the base prim itself), unless as 
mentioned they are rooted under an instancer.

### Nested instancers

It is possible for an instancer itself to be instanced by another, and thus have both the
`instancerTopology` and the `instancedBy` data sources. Note that this does not preclude
such a prototyped instancer from also drawing geometry itself. If the prototyped instancer 
is a child of the instancing instancer, then yes, such a nested instancer will not draw by
itself, and will be instance-drawn through the parent instancer. However, if the prototyped
instancer has no parent instancer, but it is instanced by another instancer somewhere else
in the hierarchy, then both the prototyped instancer will draw as if it were by itself, but
also be instance-drawn by the other instancer.

This nesting and composition of instancers is what leads to most of the complexity of point
instancing selection highlighting. Using the `instancerTopology/prototypes`, the
`instancedBy/paths`, and in some cases the `instancedBy/prototypeRoots`, we can view such
networks of instancers as a graph to be traversed.

### Implementation for point instancer and instance selection