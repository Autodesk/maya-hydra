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

#ifndef FVP_PI_INSTANCER_WH_SI_H
#define FVP_PI_INSTANCER_WH_SI_H

#include "fvpBaseWhSi.h"

PXR_NAMESPACE_OPEN_SCOPE
class HdSelectionsSchema;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class PiInstancerWhSi;
typedef PXR_NS::TfRefPtr<PiInstancerWhSi> PiInstancerWhSiRefPtr;
typedef PXR_NS::TfRefPtr<const PiInstancerWhSi> PiInstancerWhSiConstRefPtr;

/// \class PiInstancerWhSi
///
/// Wireframe selection highlight scene index for point instancer prims.
/// This scene index handles highlights for both point instancers as a whole
/// as well as individual point instances, as the selections for both of these
/// occur on the instancer prims.
///
/// A selection's sub-hierarchy contains the parent prims of the instancer,
/// the instancer itself, and its children, as well as the parent prims of each
/// of the instancer's prototypes, the prototypes themselves, and their children.
///
/// Highlight_<selectionIdentifier>
/// |__<pointInstancerParents>
/// |  |__<pointInstancer>
/// |     |__<pointInstancerChildren>
/// |     |__<prototype1Parents>
/// |        |__<prototype1>
/// |           |__<prototype1Children>
/// |__<prototype2Parents>
///    |__<prototype2>
///       |__<prototype2Children>
///
/// Specific instance selections are handled by applying a mask on the instancer highlight.
///
/// While very similar to PiPrototypeWhSi, this class also needs to create highlights when 
/// a parent of an instancer is selected, which is why the _pointInstancerPaths set exists.
///
class PiInstancerWhSi 
    : public BaseWhSi
{
public:
    FVP_API
    static PiInstancerWhSiRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& highlightHierarchyPrefix,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    ~PiInstancerWhSi() override = default;

protected:
    FVP_API
    PiInstancerWhSi(
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
    struct SelectionData {
        PrimSelection _primSelection;
        PXR_NS::SdfPathSet _instancerPaths;
        PXR_NS::SdfPathSet _prototypePaths;
        int _leadInstanceIndex;
        std::vector<int> _activeInstanceIndices;
    };

    std::set<PXR_NS::SdfPath> _pointInstancerPaths;
    std::map<SelectionKey, SelectionData> _selections;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _instancerPathsToSelections;
    std::map<PXR_NS::SdfPath, std::set<SelectionKey>> _prototypePathsToSelections;

    // Create a selection highlight hierarchcy for a single selection in the
    // instancer.  The first overload is a lower-performance convenience that
    // calls the second overload.  For large numbers of selected instances,
    // calling these methods once per instance is very costly in created prims.
    // The overload with the HdSelectionsSchema argument should be used instead.
    void _CreateSelectionHighlight(const PXR_NS::SdfPath& instancerPath, const std::string& selectionId);
    void _CreateSelectionHighlight(
        const PXR_NS::HdSceneIndexPrim&   instancerPrim,
        const PXR_NS::SdfPath&            instancerPath,
        const PXR_NS::HdSelectionsSchema& selectionsSchema,
        const std::string&                selectionId,
        const PXR_NS::VtBoolArray&        instanceMask = PXR_NS::VtBoolArray());

    // Convenience method to create a selection highlight for all selections in
    // the instancer.
    void _CreateSelectionHighlight(
        const PXR_NS::HdSceneIndexPrim&   instancerPrim,
        const PXR_NS::SdfPath&            instancerPath,
        const PXR_NS::HdSelectionsSchema& selectionsSchema
    );
    
    void _DeleteSelectionHighlight(const PXR_NS::SdfPath& instancerPath, std::string selectionId);

    // Create a full highlight if the instancer argument is fully selected (or
    // has a fully selected ancestor), or one or more point highlights if a
    // selections schema can be created on the instancer.  Return true if a
    // highlight was created.
    inline bool _ConditionallyCreateSelectionHighlight(
        const PXR_NS::HdSceneIndexPrim& instancerPrim,
        const PXR_NS::SdfPath&          instancerPath
    );
};

}

#endif // FVP_PI_INSTANCER_WH_SI_H
