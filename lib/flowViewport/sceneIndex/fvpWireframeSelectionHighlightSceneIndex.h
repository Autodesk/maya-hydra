// Copyright 2023 Autodesk
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
#ifndef FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX_H
#define FVP_WIREFRAME_SELECTION_HIGHLIGHT_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"

#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/usdImaging/usdImaging/rerootingSceneIndex.h>
#include <pxr/usd/sdf/path.h>

#include <set>
#include <unordered_map>

namespace FVP_NS_DEF {

class Selection;

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class WireframeSelectionHighlightSceneIndex;
typedef PXR_NS::TfRefPtr<WireframeSelectionHighlightSceneIndex> WireframeSelectionHighlightSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const WireframeSelectionHighlightSceneIndex> WireframeSelectionHighlightSceneIndexConstRefPtr;

/// \class WireframeSelectionHighlightSceneIndex
///
/// Uses Hydra HdRepr to add wireframe representation to selected objects
/// and their descendants.
///
class WireframeSelectionHighlightSceneIndex 
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<WireframeSelectionHighlightSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static PXR_NS::HdSceneIndexBaseRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const SelectionConstPtr& selection,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    static
    const PXR_NS::HdDataSourceLocator& ReprSelectorLocator();

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

    // Add a scene root to the set of excluded scene roots.  These are Hydra
    // scene index prim hierarchies for which wireframe selection highlighting
    // of meshes is not desired.
    FVP_API
    void addExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot);

    FVP_API
    PXR_NS::SdfPath GetSelectionHighlightPath(const PXR_NS::SdfPath& path) const;

protected:

    FVP_API
    WireframeSelectionHighlightSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const SelectionConstPtr& selection,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;

private:

    void _DirtySelectionHighlightRecursive(
        const PXR_NS::SdfPath&                            primPath, 
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries* highlightEntries
    );

    bool _IsExcluded(const PXR_NS::SdfPath& sceneRoot) const;

    std::set<PXR_NS::SdfPath> _excludedSceneRoots;
    PXR_NS::HdContainerDataSourceHandle _HighlightSelectedPrim(const PXR_NS::HdContainerDataSourceHandle& dataSource, const PXR_NS::SdfPath& primPath, const PXR_NS::HdContainerDataSourceHandle& highlightDataSource) const;

    PXR_NS::SdfPath _FindSelectionHighlightMirrorAncestor(const PXR_NS::SdfPath& path) const;
    void _CollectSelectionHighlightMirrors(const PXR_NS::SdfPath& originalPrimPath, PXR_NS::SdfPathSet& outSelectionHighlightMirrors, PXR_NS::HdSceneIndexObserver::AddedPrimEntries& outAddedPrims);
    void _RemoveSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath);
    void _CreateSelectionHighlightMirrorsForInstancer(const PXR_NS::SdfPath& instancerPath);

    const SelectionConstPtr   _selection;
    const std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;

    std::unordered_map<PXR_NS::SdfPath, PXR_NS::SdfPathSet, PXR_NS::SdfPath::Hash> _selectionHighlightMirrorsByInstancer;
    std::unordered_map<PXR_NS::SdfPath, size_t, PXR_NS::SdfPath::Hash> _selectionHighlightMirrorUseCounters;
    std::set<PXR_NS::SdfPath> _highlightedPrototypePaths;
};

}

#endif
