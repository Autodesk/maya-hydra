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

#include <functional>
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
/// one UsdImaging resolves for the prim, which falls back to the extentsHint when no extent is
/// authored. Prims without a valid, non-empty extent get no box.
///
/// The box is a basisCurves prim named __unloadedPayloadBounds, and a child of the unloaded prim. It
/// takes the transform, visibility and prim origin of the unloaded prim, so it is placed like it and
/// picking it picks the unloaded prim. It is drawn in the secondary graphics pass, in every display
/// style, in the wireframe color of the unloaded prim, which follows the selection.
///
/// UsdImaging's built-in displayUnloadedPrimsWithBounds is deliberately not used, because it:
/// - has no selection color, so the box does not follow the selection;
/// - cannot be toggled at run time, which the "USD Payload Bounding Boxes" Show-menu filter needs;
/// - leaves a stale box behind when the payload is loaded, since USD 25.08.
/// - the UsdImagingDrawModeSceneIndex strips the isLoaded data from the prim, making the drawn
///   boxes impossible to differentiate from any other basisCurves.
///
/// The input transforms and visibilities must be flattened, and this scene index must come after
/// the selection scene index, whose dirty notifications it relies on.
class UnloadedPayloadBoundsSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<UnloadedPayloadBoundsSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    /// Whether the prim at primPath, or any of its descendants, may be unloaded. It is called for
    /// every added prim, so must be cheap, and must reflect the current state of the input, e.g.
    /// a USD stage that is loaded with all its descendants has no unloaded prims.
    using MayHaveUnloadedPrimsFn = std::function<bool(const PXR_NS::SdfPath& primPath)>;

    FVP_API
    static UnloadedPayloadBoundsSceneIndexRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        const MayHaveUnloadedPrimsFn&                   mayHaveUnloadedPrims,
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

    /// Makes the boxes at or under \p primPaths re-pull their wireframe color. By default, all
    /// boxes are dirtied, e.g. after a color preference change. Pass specific paths to dirty only
    /// some boxes, e.g. when the lead object changes. Selection changes are handled without
    /// calling this, but a lead change is not a selection change, so it is not.
    FVP_API
    void DirtyWireframeColors(
        const PXR_NS::SdfPathVector& primPaths = { PXR_NS::SdfPath::AbsoluteRootPath() });

protected:
    FVP_API
    UnloadedPayloadBoundsSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr&           inputSceneIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface,
        const MayHaveUnloadedPrimsFn&                   mayHaveUnloadedPrims,
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

    // Starts or stops tracking primPath, depending on whether it is the root of an unloaded
    // subtree.
    void _UpdateTracking(const PXR_NS::SdfPath& primPath);
    void _StopTracking(const PXR_NS::SdfPath& primPath);

    // Appends a wireframe color dirty entry for each box at or under primPath.
    void _AddColorDirtiesUnder(
        const PXR_NS::SdfPath&                            primPath,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& inoutEntries) const;

    const std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;
    const MayHaveUnloadedPrimsFn                   _mayHaveUnloadedPrims;
    bool                                           _enabled;

    // Roots of unloaded subtrees, sorted so that the ones under a path are a contiguous range.
    std::set<PXR_NS::SdfPath> _unloadedPrims;
    // The subset of _unloadedPrims with a valid extent.
    std::set<PXR_NS::SdfPath> _unloadedPrimsWithExtents;
};

} // namespace FVP_NS_DEF

#endif // FVP_UNLOADED_PAYLOAD_BOUNDS_SCENE_INDEX_H
