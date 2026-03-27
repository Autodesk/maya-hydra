// Copyright 2026 Autodesk
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

#ifndef FVP_GENERATIVE_PROCEDURAL_WH_SI_H
#define FVP_GENERATIVE_PROCEDURAL_WH_SI_H

#include "fvpBaseWhSi.h"

#include <set>
#include <functional>

PXR_NAMESPACE_OPEN_SCOPE
class HdSelectionsSchema;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class GenerativeProceduralWhSi;
typedef PXR_NS::TfRefPtr<GenerativeProceduralWhSi>       GenerativeProceduralWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const GenerativeProceduralWhSi> GenerativeProceduralWhSiConstRefPtr;

class GenerativeProceduralWhSi : public BaseWhSi
{
public:
    FVP_API
    static GenerativeProceduralWhSiRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const PXR_NS::SdfPath&                          highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface);

    FVP_API
    ~GenerativeProceduralWhSi() override = default;

    FVP_API
    void SetTerminalSceneIndexGetter(std::function<PXR_NS::HdSceneIndexBaseRefPtr()> getter);


protected:

    FVP_API
    GenerativeProceduralWhSi(
        const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const PXR_NS::SdfPath&                          highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface);

    FVP_API
    PXR_NS::HdSceneIndexPrim GetHighlightPrim(
        const PXR_NS::SdfPath& selectionPath,
        const PXR_NS::SdfPath& fullPrimPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetHighlightChildPrimPaths(
        const PXR_NS::SdfPath& selectionPath,
        const PXR_NS::SdfPath& fullPrimPath) const override;

    FVP_API
    void ProcessAddedPrims(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    FVP_API
    void ProcessRemovedPrims(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    FVP_API
    void ProcessDirtiedPrims(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

    FVP_API
    void ProcessFullySelectedChange(const PXR_NS::SdfPath& primPath, bool isFullySelected) override;

private:

    std::set<PXR_NS::SdfPath> _generativeProceduralPaths;

    void _CreateSelectionHighlight(const PXR_NS::SdfPath& generativeProceduralPath);

    void _DeleteSelectionHighlight(const PXR_NS::SdfPath& generativeProceduralPath);

    std::function<PXR_NS::HdSceneIndexBaseRefPtr()> _terminalSceneIndexGetter;
};

} // namespace FVP_NS_DEF

#endif // FVP_GENERATIVE_PROCEDURAL_WH_SI_H
