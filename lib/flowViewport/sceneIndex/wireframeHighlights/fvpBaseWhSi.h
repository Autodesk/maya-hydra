// Copyright 2025 Autodesk
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef FVP_BASE_WH_SI_H
#define FVP_BASE_WH_SI_H

#include <flowViewport/api.h>
#include <flowViewport/fvpUtils.h>
#include <flowViewport/fvpWireframeColorInterface.h>
#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>

#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/selectionSchema.h>

#include <functional>
#include <set>

namespace FVP_NS_DEF {

using SelectionKey = std::pair<PXR_NS::SdfPath, std::string>;

enum InstancingPathsCollectionDirection {
    None = 0,
    Prototypes = 1 << 0,
    InstancedBy = 1 << 1,
    Bidirectional = Prototypes | InstancedBy
};

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class BaseWhSi;
typedef PXR_NS::TfRefPtr<BaseWhSi> BaseWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const BaseWhSi> BaseWhSiConstRefPtr;

/// \class BaseWhSi
///
/// Base class for wireframe selection highlighting scene indices.
///
/// A selection highlight is composed of four parts :
/// 1. A highlight hierarchy prefix (SdfPath)
/// 2. A prim path (SdfPath)
/// 3. A selection identifier (string)
/// 4. The selection highlight sub-hierarchy (Hydra prims)
///
/// Which are represented in Hydra as follows :
/// <highlightHierarchyPrefix>
/// |__<primPath>
///    |__Highlight_<selectionIdentifier>
///       |__<selectionHighlightSubHierarchy>
///
/// The BaseWhSi class is responsible for composing the four parts
/// together; derived classes only need to register and unregister
/// selection highlights as needed (through unique pairs composed of
/// a prim path + a selection identifier), and handle the processing
/// of their selection highlight sub-hierarchies.
///
/// To do this, the BaseWhSi class overrides the typical HdSceneIndex
/// methods (GetPrim, GetChildPrimPaths, _PrimsAdded, _PrimsRemoved,
/// _PrimsDirtied), but does not allow for derived classes to override
/// them in turn. Instead, analogous, but more specialized abstract methods
/// must be implemented by the derived classes. These are :
///
/// 1. GetHighlightPrim & GetHighlightChildPrimPaths
/// These methods are implemented by the derived scene index to return the
/// selection highlight sub-hierarchies. This allows derived scene indices to
/// focus only on handling their specific flavor of selection highlighting.
/// These methods will be automatically called by BaseWhSi::GetPrim and
/// BaseWhSi::GetChildPrimPaths.
///
/// 2. ProcessAddedPrims, ProcessRemovedPrims & ProcessDirtiedPrims
/// These are essentialy the same as the typical _PrimsAdded, _PrimsRemoved,
/// & _PrimsDirtied. The only difference is that the BaseWhSi class will
/// not forward notifications for excluded paths. Having these separate methods
/// also allows the BaseWhSi to do some of its own processing beforehand, which
/// allows it to provide this next, optional method :

/// 3. ProcessFullySelectedChange
/// This is an optional method that can be overriden by derived classes if desired.
/// This method will be called whenever a prim becomes fully selected, or when it
/// stops being so. This is useful for handling selection highlights for when a
/// parent prim is selected/deselected.
///
class BaseWhSi
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<BaseWhSi>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const final;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const final;

    FVP_API
    void AddExcludedPath(const PXR_NS::SdfPath& path);

protected:
    FVP_API
    BaseWhSi(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) final;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) final;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) final;

    FVP_API
    bool IsExcludedPath(const PXR_NS::SdfPath& path) const;

    FVP_API
    PXR_NS::SdfPath SelectionPathFromKey(const SelectionKey& selectionKey) const;

    FVP_API
    SelectionKey SelectionKeyFromPath(const PXR_NS::SdfPath& selectionPath) const;

    // Registers a selection highlight and returns the path to the parent prim of
    // the selection highlight sub-hierarchy.
    FVP_API
    PXR_NS::SdfPath RegisterSelection(const SelectionKey& selectionKey);

    // Unegisters a selection highlight and returns the path to the parent prim of
    // the selection highlight sub-hierarchy.
    FVP_API
    PXR_NS::SdfPath UnregisterSelection(const SelectionKey& selectionKey);

    FVP_API
    virtual PXR_NS::HdSceneIndexPrim GetHighlightPrim(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const = 0;

    FVP_API
    virtual PXR_NS::SdfPathVector GetHighlightChildPrimPaths(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const = 0;

    FVP_API
    virtual void ProcessAddedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) = 0;

    FVP_API
    virtual void ProcessRemovedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) = 0;

    FVP_API
    virtual void ProcessDirtiedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) = 0;

    // Optional helper method that can be overriden by derived classes;
    // this can help with handling highlights for when a parent prim is selected/deselected.
    FVP_API
    virtual void ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected);

    // Returns whether or not this prim or one of its parents is fully selected.
    FVP_API
    bool HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath);

    // Executes a given operation on a prim and all its descendants within the same hierarchy
    // (prototypes are considered separate hierarchies). When running the operation on a given
    // prim returns false, the prim's descendants are skipped.
    FVP_API
    void ForEachPrimInHierarchy(const PXR_NS::SdfPath& hierarchyRoot, const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation) const;

    // Collect the paths to the instancers and prototypes of the instancing graph/network that the given prim is a part of.
    // The direction parameter allows for specifying whether to only collect instancers, prototypes, or both.
    FVP_API
    void CollectInstancingPaths(const PXR_NS::SdfPath& primPath, InstancingPathsCollectionDirection direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const;

    // Make the given prim be drawn as a wireframe of the given color.
    PXR_NS::HdContainerDataSourceHandle SetWireframeRepr(const PXR_NS::HdContainerDataSourceHandle& dataSource, const PXR_NS::GfVec4f& color) const;

#if PXR_VERSION >= 2405
    // Given a mesh and geomSubset data sources, edits and returns the mesh data source to fit the given geomSubset
    FVP_API
    PXR_NS::HdContainerDataSourceHandle MakeGeomSubsetHighlight(
        const PXR_NS::HdContainerDataSourceHandle& meshPrimDataSource,
        const PXR_NS::HdContainerDataSourceHandle& geomSubsetPrimDataSource) const;
#endif

    const PXR_NS::SdfPath _highlightHierarchyPrefix;
    const std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;

    std::set<PXR_NS::SdfPath> _excludedPaths;

    std::set<PXR_NS::SdfPath> _fullySelectedPaths;

    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _primPathsToSelections;
    std::set<PXR_NS::SdfPath> _selectionPaths;
};

// Repath instancing-related data sources by replacing srcPrefix with dstPrefix.
// Mainly used to setup selection highlight instancers and instances.
PXR_NS::HdContainerDataSourceHandle RepathInstancingDataSources(
    const PXR_NS::HdContainerDataSourceHandle& primDataSource,
    const PXR_NS::SdfPath& srcPrefix,
    const PXR_NS::SdfPath& dstPrefix);

#if PXR_VERSION >= 2405
// Edit the given mesh data source such that its topology matches the given geomSubset.
PXR_NS::HdContainerDataSourceHandle
TrimMeshForGeomSubset(const PXR_NS::HdContainerDataSourceHandle& meshPrimDataSource, const PXR_NS::HdContainerDataSourceHandle& geomSubsetPrimDataSource);
#endif

}

#endif // FVP_BASE_WH_SI_H
