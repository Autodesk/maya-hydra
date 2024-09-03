//
// Copyright 2024 Autodesk
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
#ifndef FLOW_VIEWPORT_SCENE_INDEX_ISOLATE_SELECT_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENE_INDEX_ISOLATE_SELECT_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"

#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h" // For PrimSelections

//Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class IsolateSelectSceneIndex;
typedef PXR_NS::TfRefPtr<IsolateSelectSceneIndex> IsolateSelectSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const IsolateSelectSceneIndex> IsolateSelectSceneIndexConstRefPtr;

/// \class IsolateSelectSceneIndex
///
/// A filtering scene index that changes the visibility of prims that are not
/// in its set to false.
///
/// The input isolate select data to the isolate select scene index is a set of
/// scene index prim selections tracked as an Fvp::Selection.  External code is
/// responsible for converting the application isolate selection into prim
/// selections.
///
/// Isolate select does not remove prims from the scene, it hides them.  This
/// matches the Maya algorithm.  A prim's previous visibility is restored
/// simply by taking out the isolate select scene index, thereby allowing
/// the original visibility to be sent to the renderer unchanged.
///
/// At time of writing a single isolate select scene index is used to service
/// all viewports in the application, by switching the isolate selection on the
/// isolate scene index using IsolateSelectSceneIndex::SetViewport().
///
/// IsolateSelectSceneIndex::GetPrim() passes through prims that have an
/// ancestor or descendant (including themselves) in the isolate selection.
/// Other prims are hidden by setting visibility off.
///
/// When the isolate selection is changed, prim visibility in the scene is
/// dirtied in the following way: starting at the changed prim path,
/// - Dirty all sibling visibilities
/// - Move up to the prim's parent
/// - Recurse and dirty all sibling visibilities.
/// - End recursion at the scene root.
///
/// Dirtying any prim's visibility recurses to its children, to dirty the
/// visibility for the entire subtree.
///
/// For example, consider the following hierarchy:
///
/// a
/// |_b
///   |_c
///   |_d
/// |_e
///   |_f
///   |_g
///     |_h
///     |_i
///     |_j
/// |_k
///
/// Given an initially empty isolate selection, adding f to the isolate
/// selection will:
///
/// - Dirty g's visibility, and recursively that of all its descendants.
/// - Recursing up to e, dirty b and k's visibility, and all their descendants.
/// - Recursing up to a (the root), the algorithm ends.
///
class IsolateSelectSceneIndex :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<IsolateSelectSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    static constexpr char kDisplayName[] = "Flow Viewport Isolate Select Scene Index";

    FVP_API
    static IsolateSelectSceneIndexRefPtr New(
        const std::string&                    viewportId,
        const SelectionPtr&                   isolateSelection,
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex
    );

    // From HdSceneIndexBase
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

    ~IsolateSelectSceneIndex() override = default;

    FVP_API
    void AddIsolateSelection(const PrimSelections& primSelections);
    FVP_API
    void RemoveIsolateSelection(const PrimSelections& primSelections);
    FVP_API
    void ReplaceIsolateSelection(const SelectionConstPtr& selection);
    FVP_API
    void ClearIsolateSelection();

    // Set viewport information (viewport ID and isolate selection) for this
    // scene index.  This occurs when switching the single scene index between
    // viewports.  If the same viewport ID or isolate selection is given as an
    // argument, a warning will be issued.  Otherwise, the previous and the new
    // isolate selections will be dirtied.
    FVP_API
    void SetViewport(
        const std::string&  viewportId,
        const SelectionPtr& isolateSelection
    );

    // Get viewport information for this scene index.
    FVP_API
    std::string GetViewportId() const;
    FVP_API
    SelectionPtr GetIsolateSelection() const;

protected:
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

private:

    IsolateSelectSceneIndex(
        const std::string&                    viewportId,
        const SelectionPtr&                   isolateSelection,
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex
    );

    void _DirtyVisibility(
        const PXR_NS::SdfPath&                            primPath,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries* dirtiedEntries
    ) const;

    void _DirtyVisibilityRecursive(
        const PXR_NS::SdfPath&                            primPath,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries* dirtiedEntries
    ) const;

    void _ReplaceIsolateSelection(const SelectionConstPtr& selection);

    std::string  _viewportId;

    SelectionPtr _isolateSelection{};
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENE_INDEX_ISOLATE_SELECT_SCENE_INDEX_H
