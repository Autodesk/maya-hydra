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
#ifndef FVP_PI_PROTOTYPE_WH_SI_H
#define FVP_PI_PROTOTYPE_WH_SI_H

#include "baseWhSi.h"
#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"
#include "flowViewport/sceneIndex/wireframeHighlights/baseWhSi.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <set>
#include <unordered_map>

// Each WhSi handles the highlighting for all selections for a given type of prim. i.e. instancer and instance selections both occur on the same instancer prim, therefore
//these two types of selection should be handled by the same scene index. Why? Because the selections can change order unpredictably, so two separate scene indices would have
// to account for this. Actually this can totally be done without using the same scene index so forget it.

namespace FVP_NS_DEF {

enum SelectionHighlightsCollectionDirection3 {
    None3 = 0,
    Prototypes3 = 1 << 0,
    InstancedBy3 = 1 << 1,
    Bidirectional3 = Prototypes3 | InstancedBy3
};

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class PiPrototypeWhSi;
typedef PXR_NS::TfRefPtr<PiPrototypeWhSi> PiPrototypeWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const PiPrototypeWhSi> PiPrototypeWhSiConstRefPtr;

/// \class PiPrototypeWhSi
///
/// Uses Hydra HdRepr to add wireframe representation to selected objects
/// and their descendants.
///
class PiPrototypeWhSi 
    : public BaseWhSi
{
public:
    FVP_API
    static PXR_NS::HdSceneIndexBaseRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

protected:
    FVP_API
    PiPrototypeWhSi(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    PXR_NS::HdSceneIndexPrim GetHighlightPrim(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetHighlightChildPrimPaths(const PXR_NS::SdfPath &selectionPath, const PXR_NS::SdfPath &fullPrimPath) const override;

    FVP_API
    void ProcessAddedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    FVP_API
    void ProcessRemovedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    FVP_API
    void ProcessDirtiedPrims(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;

private:
    //std::map<SelectionKey, SelectionData> _selections;
    std::map<PXR_NS::SdfPath, PXR_NS::SdfPath> _selectionPathsToPrototypePrefixes;
    std::map<PXR_NS::SdfPath, PXR_NS::SdfPath> _selectionPathsToPrototypePaths;
    std::set<PXR_NS::SdfPath, size_t> _prototypePathsSelectionsCount;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _prototypePathsToSelections;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _instancerPathsToSelections;


    //std::map<PXR_NS::SdfPath, PXR_NS::SdfPathSet> _prototypePathsToSelectionPaths;
    //std::set<PXR_NS::SdfPath> _fullySelectedPaths;
    //std::set<PXR_NS::SdfPath> _instancePaths;
    //std::set<PXR_NS::SdfPath> _highlightedInstancePaths;

    void _CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId);
    void _DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId);

    void _CollectInstancingPaths(const PXR_NS::SdfPath& primPath, SelectionHighlightsCollectionDirection3 direction, PXR_NS::SdfPathSet& outInstancerPaths, PXR_NS::SdfPathSet& outPrototypePaths) const;
    void _ForEachPrimInHierarchy(const PXR_NS::SdfPath& hierarchyRoot, const std::function<bool(const PXR_NS::SdfPath&, const PXR_NS::HdSceneIndexPrim&)>& operation) const;
};

PXR_NS::HdContainerDataSourceHandle MakeWireframe2(const PXR_NS::HdContainerDataSourceHandle& dataSource, const PXR_NS::GfVec4f& color);

}

#endif // FVP_PI_PROTOTYPE_WH_SI_H
