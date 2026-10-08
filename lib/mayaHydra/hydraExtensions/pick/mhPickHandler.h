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
#ifndef MH_PICK_HANDLER_H
#define MH_PICK_HANDLER_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/pick/mhPickHandlerFwd.h>
#include <mayaHydraLib/pick/mhPickHitFwd.h>

#include <pxr/pxr.h>

#include <maya/MApiNamespace.h>

#include <ufe/namedSelection.h>
#include <ufe/sceneItemList.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

/// \class PickHandler
///
/// The pick handler performs the picking to selection mapping, from the Hydra
/// scene index pick result to the Maya-centric selection output.
///
/// The pick handler takes the Hydra scene index pick result, with its Hydra
/// scene index path, computes the corresponding Maya application scene item
/// from it, and places the Maya scene item in either the Maya selection list
/// (for Maya DG items) or UFE selection (non Maya DG items).

class PickHandler
{
public:

    struct Input;
    struct Output;

    MAYAHYDRALIB_API
    virtual bool handlePickHit(
        const Input& pickInput, Output& pickOutput
    ) const = 0;

    MAYAHYDRALIB_API
    virtual bool inSingleNodeComponentsPick(const PickHit&) const {
        return false;
    }

    /// The scene items that a click on \p pickHit would select, resolved as handlePickHit()
    /// resolves them, without changing the selection. Hover highlighting uses it to show exactly
    /// what a click would select. \p isSolePickHit is true when \p pickHit is the only hit of the
    /// pick: a click, or a marquee over a single prim. Hover passes true. It matters only for the
    /// Faces GeomSubsets pick mode: a sole hit that hits no GeomSubset falls back to the prim or
    /// instance, while one hit among several then resolves to nothing, so that a marquee selects
    /// GeomSubsets only. Returns false when the handler does not support it: the caller then falls
    /// back to the picked prim. A handler that supports it may still resolve the hit to no item,
    /// as a click would select nothing.
    MAYAHYDRALIB_API
    virtual bool resolvePickHit(
        const PickHit& /* pickHit */,
        bool           /* isSolePickHit */,
        Ufe::SceneItemList& /* sceneItems */
    ) const {
        return false;
    }
};

/// \class PickHandler::Input
///
/// Picking input consists of the Hydra pick hit and the Maya selection state.
struct PickHandler::Input {
    Input(
        const PickHit&                   pickHitArg, 
        const MHWRender::MSelectionInfo& pickInfoArg,
        const bool                       isSolePickHitArg
    ) : pickHit(pickHitArg), pickInfo(pickInfoArg), isSolePickHit(isSolePickHitArg) {}

    const PickHit&                   pickHit;
    const MHWRender::MSelectionInfo& pickInfo;
    const bool                       isSolePickHit;
};

/// \class PickHandler::Output
///
/// Picking output can go either to the UFE representation of the Maya selection
/// (which supports non-Maya objects), or the classic MSelectionList
/// representation of the Maya selection (which only supports Maya objects). It
/// is up to the implementer of the pick handler to decide which is used. If the
/// Maya selection is used, there must be a world space hit point in one to one
/// correspondence with each Maya selection item placed into the MSelectionList.
struct PickHandler::Output {
    Output(
        MSelectionList&                 mayaSn,
        MPointArray&                    worldSpaceHitPts,
        const Ufe::NamedSelection::Ptr& ufeSn
    ) : mayaSelection(mayaSn), mayaWorldSpaceHitPts(worldSpaceHitPts),
        ufeSelection(ufeSn) {}

    MSelectionList&                 mayaSelection;
    MPointArray&                    mayaWorldSpaceHitPts;
    const Ufe::NamedSelection::Ptr& ufeSelection;
};

}

#endif
