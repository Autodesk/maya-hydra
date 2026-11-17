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
#ifndef FVP_UNLOADED_PAYLOAD_BOUNDS_SCENE_INDEX_H
#define FVP_UNLOADED_PAYLOAD_BOUNDS_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/fvpWireframeColorInterface.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <memory>
#include <set>

namespace FVP_NS_DEF {

class UnloadedPayloadBoundsSceneIndex;
typedef PXR_NS::TfRefPtr<UnloadedPayloadBoundsSceneIndex> UnloadedPayloadBoundsSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const UnloadedPayloadBoundsSceneIndex> UnloadedPayloadBoundsSceneIndexConstRefPtr;

/// \class UnloadedPayloadBoundsSceneIndex
///
/// Draws the unloaded prims of USD stages as wireframe bounding boxes.
///
/// The input must report unloaded prims, i.e. the UsdImagingStageSceneIndex must be created with
/// includeUnloadedPrims. A prim gets a box when UsdImaging reports it as not loaded and its parent
/// as loaded, so an unloaded subtree gets one box, from the extent of its root. That extent is the
/// authored extent of the prim. Prims without a valid, non-empty extent get no box.
///
/// The box is a basisCurves prim named __unloadedPayloadBounds, and a child of the unloaded prim. It
/// takes the transform, visibility and prim origin of the unloaded prim, so it is placed like it and
/// picking it picks the unloaded prim. It is drawn in the secondary graphics pass, in every display
/// style, in the wireframe color of the unloaded prim, which follows the selection.
///
/// The input transforms and visibilities must be flattened, and this scene index must come after
/// the selection scene index, whose dirty notifications it relies on.
class UnloadedPayloadBoundsSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<UnloadedPayloadBoundsSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    /// No box is created under \p excludedSceneRoots, which skips looking for unloaded prims in
    /// subtrees known to have none.
    FVP_API
    static UnloadedPayloadBoundsSceneIndexRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        const PXR_NS::SdfPathVector&                    excludedSceneRoots,
        bool                                            enabled);

    FVP_API
    ~UnloadedPayloadBoundsSceneIndex() override = default;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

    /// Show or hide all the boxes.
    FVP_API
    void SetEnabled(bool enabled);
    bool IsEnabled() const { return _enabled; }

    /// Makes the boxes re-pull their wireframe color, e.g. after a color preference change.
    /// Selection changes are handled without calling this.
    FVP_API
    void DirtyWireframeColors();

protected:
    FVP_API
    UnloadedPayloadBoundsSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        const PXR_NS::SdfPathVector&                    excludedSceneRoots,
        bool                                            enabled);

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

private:
    bool _IsTracked(const PXR_NS::SdfPath& primPath) const
    {
        return _unloadedPrims.find(primPath) != _unloadedPrims.end();
    }
    bool _HasBox(const PXR_NS::SdfPath& primPath) const
    {
        return _enabled && _unloadedPrimsWithExtents.find(primPath) != _unloadedPrimsWithExtents.end();
    }
    bool _IsExcluded(const PXR_NS::SdfPath& primPath) const;

    // Starts or stops tracking primPath, depending on whether it is the root of an unloaded
    // subtree.
    void _UpdateTracking(const PXR_NS::SdfPath& primPath);

    // Appends a wireframe color dirty entry for each box at or under primPath.
    void _AddColorDirtiesUnder(
        const PXR_NS::SdfPath&                            primPath,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& inoutEntries) const;

    const std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;
    const PXR_NS::SdfPathVector                    _excludedSceneRoots;
    bool                                           _enabled;

    // Roots of unloaded subtrees, sorted so that the ones under a path are a contiguous range.
    std::set<PXR_NS::SdfPath> _unloadedPrims;
    // The subset of _unloadedPrims with a valid extent.
    std::set<PXR_NS::SdfPath> _unloadedPrimsWithExtents;
};

} // namespace FVP_NS_DEF

#endif // FVP_UNLOADED_PAYLOAD_BOUNDS_SCENE_INDEX_H
