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
#ifndef FVP_PRIM_REMOVAL_ENFORCING_SCENE_INDEX_H
#define FVP_PRIM_REMOVAL_ENFORCING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/pathTable.h>

namespace FVP_NS_DEF {

class PrimRemovalEnforcingSceneIndex;
typedef PXR_NS::TfRefPtr<PrimRemovalEnforcingSceneIndex> PrimRemovalEnforcingSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const PrimRemovalEnforcingSceneIndex> PrimRemovalEnforcingSceneIndexConstRefPtr;

/// \class PrimRemovalEnforcingSceneIndex
///
/// A scene index that enforces coherent behaviour between prim removal
/// and prim retrieval. Essentially, when this scene index receives a
/// PrimsRemoved notification, it will ensure that the removed prim and
/// all its children will appear inexistent to downstream scene indices,
/// until they are properly added back in again through a PrimsAdded
/// notification.
///
/// While this should likely be expected behavior from any scene index,
/// scene indices often just defer to their input scene index, so it should
/// be on the first scene index that sends a PrimsRemoved notification to
/// handle this properly; and OpenUSD's UsdImagingDrawModeSceneIndex does not 
/// fulfill this expectation. This can lead to errors when combined with 
/// HdMergingSceneIndex::_PrimsRemoved's behavior.
/// 
/// The problematic scenario is as follows :
/// 1. UsdImagingDrawModeSceneIndex receives PrimsAdded notifications.
/// 2. UsdImagingDrawModeSceneIndex sends out PrimsRemoved notifications to 
///    deal with an edge case (explained here : 
///    https://github.com/PixarAnimationStudios/OpenUSD/blob/v25.08/pxr/usdImaging/usdImaging/drawModeSceneIndex.cpp#L271-L300)
/// 3. HdMergingSceneIndex receives the PrimsRemoved notifications.
/// 4. HdMergingSceneIndex checks if a prim needs to actually be fully removed
///    or if it needs to be resynced, as the prim could also be present in the 
///    other input scenes of the merging scene index. To do this, it calls GetPrim 
///    and GetChildPrimPaths on itself, and if a prim has a valid type, data source
///    or child paths, then it is considered valid and resynced instead of removed.
///    However, this GetPrim/GetChildPrimPaths call will end up recursing into all
///    of the merging scene index's input scenes, including the UsdImagingDrawModeSceneIndex,
///    which will (incorrectly) return a valid prim and child prim paths.
/// 5. HdMergingSceneIndex considers the prim to still be present, and sends out
///    PrimsAdded notifications to resync.
/// 6. Any downstream scene index observer will receive the PrimsAdded notifications
///    and will resync, most likely calling GetPrim/GetChildPrimPaths and again
///    retrieving the valid-but-should-not-be prims and paths from the 
///    UsdImagingDrawModeSceneIndex.
/// Result : The downstream observers of the scene index chain have an incorrect view
/// of the scene.
/// 
/// In our case, when combined with the HdSceneIndexAdapterSceneDelegate's special handling
/// of geomSubsets, this can lead to HdChangeTracker trying to dirty a mesh prim that does not
/// exist, leading to a failing TF_VERIFY : 
/// https://github.com/PixarAnimationStudios/OpenUSD/blob/v25.08/pxr/imaging/hd/changeTracker.cpp#L141
/// 
/// To note, this bug was previously avoided by HdMergingSceneIndex explicitly avoiding calling
/// GetPrim/GetChildPrimPaths on the scene index that sent the PrimsRemoved notifications. This
/// change is what unearthed the latent issue in UsdImagingDrawModeSceneIndex : 
/// https://github.com/PixarAnimationStudios/OpenUSD/commit/b4112c76b57c13cee4f6d7634dffef88c68c98e6
///
class PrimRemovalEnforcingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<PrimRemovalEnforcingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static PrimRemovalEnforcingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputScene);

    FVP_API
    ~PrimRemovalEnforcingSceneIndex() override = default;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

protected:

    FVP_API
    PrimRemovalEnforcingSceneIndex(PXR_NS::HdSceneIndexBaseRefPtr const& inputSceneIndex);
    
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
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

    FVP_API
    bool _PrimExists(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    bool _PathExists(const PXR_NS::SdfPath& primPath) const;

private:
    // Represents the scene visible to downstream scene indices.
    // The keys of the table represent the paths visible in the scene,
    // and the bool values represent whether the prim at a path is
    // included or not. Excluded prims have no type and no data source.
    PXR_NS::SdfPathTable<bool> _hierarchy;
};

} // namespace FVP_NS_DEF

#endif // FVP_PRIM_REMOVAL_ENFORCING_SCENE_INDEX_H
