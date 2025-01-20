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

#ifndef FVP_NI_PROTOTYPE_WH_SI_H
#define FVP_NI_PROTOTYPE_WH_SI_H

#include "baseWhSi.h"

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class NiPrototypeWhSi;
typedef PXR_NS::TfRefPtr<NiPrototypeWhSi> NiPrototypeWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const NiPrototypeWhSi> NiPrototypeWhSiConstRefPtr;

/// \class NiPrototypeWhSi
///
/// Wireframe selection highlight scene index for native instance prototypes.
///
class NiPrototypeWhSi 
    : public BaseWhSi
{
public:
    FVP_API
    static NiPrototypeWhSiRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    ~NiPrototypeWhSi() override = default;

protected:
    FVP_API
    NiPrototypeWhSi(
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
    std::map<PXR_NS::SdfPath, PXR_NS::SdfPath> _selectionPathsToPrototypePrefixes;
    std::map<PXR_NS::SdfPath, PXR_NS::SdfPath> _selectionPathsToPrototypePaths;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _prototypePathsToSelections;

    void _CreateSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId);
    void _DeleteSelectionHighlight(const PXR_NS::SdfPath& prototypePath, std::string selectionId);
};

}

#endif // FVP_NI_PROTOTYPE_WH_SI_H
