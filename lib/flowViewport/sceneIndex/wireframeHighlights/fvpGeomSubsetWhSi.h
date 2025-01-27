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

#ifndef FVP_GEOM_SUBSET_WH_SI_H
#define FVP_GEOM_SUBSET_WH_SI_H

#include "fvpBaseWhSi.h"

#if PXR_VERSION >= 2403

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class GeomSubsetWhSi;
typedef PXR_NS::TfRefPtr<GeomSubsetWhSi> GeomSubsetWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const GeomSubsetWhSi> GeomSubsetWhSiConstRefPtr;

/// \class GeomSubsetWhSi
///
/// Wireframe selection highlight scene index for geomSubset prims.
///
/// A selection's sub-hierarchy contains only the parent mesh of
/// the selected geomSubset, trimmed to fit the geomSubset.
///
/// Highlight_<selectionIdentifier>
/// |__<trimmedParentMesh>
///
class GeomSubsetWhSi 
    : public BaseWhSi
{
public:
    FVP_API
    static GeomSubsetWhSiRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    ~GeomSubsetWhSi() override = default;

protected:
    FVP_API
    GeomSubsetWhSi(
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
    
    FVP_API
    void ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected) override;

private:
    std::set<PXR_NS::SdfPath> _geomSubsetPaths;

    void _CreateSelectionHighlight(const PXR_NS::SdfPath& geomSubsetPath);
    void _DeleteSelectionHighlight(const PXR_NS::SdfPath& geomSubsetPath);
};

}

#endif // #if PXR_VERSION >= 2403

#endif // FVP_GEOM_SUBSET_WH_SI_H
