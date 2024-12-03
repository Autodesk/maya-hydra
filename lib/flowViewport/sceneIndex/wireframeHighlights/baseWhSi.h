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
#ifndef FVP_BASE_WH_SI_H
#define FVP_BASE_WH_SI_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <set>
#include <unordered_map>

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class BaseWhSi;
typedef PXR_NS::TfRefPtr<BaseWhSi> BaseWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const BaseWhSi> BaseWhSiConstRefPtr;

using SelectionKey = std::pair<PXR_NS::SdfPath, size_t>;

// struct SelectionKey {
//     PXR_NS::SdfPath primPath;
//     size_t selectionIndex;

//     inline bool operator==(const SelectionKey &rhs) const {
//         return primPath == rhs.primPath
//             && selectionIndex == rhs.selectionIndex;
//     }

//     struct Hash {
//         size_t operator()(const SelectionKey& selectionKey) const noexcept
//         {
//             size_t primPathHash = PXR_NS::SdfPath::Hash{}(selectionKey.primPath);
//             return primPathHash ^ (1ULL << selectionKey.selectionIndex);
//         }
//     };
// };

/// \class BaseWhSi
///
/// Uses Hydra HdRepr to add wireframe representation to selected objects
/// and their descendants.
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

protected:
    FVP_API
    BaseWhSi(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex,
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
    PXR_NS::SdfPath SelectionPathFromKey(const SelectionKey& selectionKey);

    FVP_API
    SelectionKey SelectionKeyFromPath(const PXR_NS::SdfPath& selectionPath);

    FVP_API
    void RegisterSelection(const SelectionKey& selectionKey);

    FVP_API
    void UnregisterSelection(const SelectionKey& selectionKey);

    FVP_API
    virtual PXR_NS::HdSceneIndexPrim GetHighlightPrim(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const = 0;

    FVP_API
    virtual PXR_NS::SdfPathVector GetHighlightChildPrimPaths(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const = 0;

    FVP_API
    virtual void ProcessAddedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) const = 0;
    
    FVP_API
    virtual void ProcessRemovedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) const = 0;
    
    FVP_API
    virtual void ProcessDirtiedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) const = 0;

    std::set<PXR_NS::SdfPath> _selectionPaths;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _primPathsToSelections;

    PXR_NS::SdfPath _highlightHierarchyPrefix;
};

PXR_NS::HdContainerDataSourceHandle SetWireframeRepr(const PXR_NS::HdContainerDataSourceHandle& dataSource, const PXR_NS::GfVec4f& color);

}

#endif // FVP_BASE_WH_SI_H
