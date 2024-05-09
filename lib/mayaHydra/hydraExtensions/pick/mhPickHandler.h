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

#include <pxr/pxr.h>

#include <maya/MApiNamespace.h>

#include <ufe/namedSelection.h>

PXR_NAMESPACE_OPEN_SCOPE
struct HdxPickHit;
PXR_NAMESPACE_CLOSE_SCOPE

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
};

/// \class PickHandler::Input
///
/// Picking input consists of the Hydra pick hit and the Maya selection state.
struct PickHandler::Input {
    Input(
        const HdxPickHit&                pickHitArg, 
        const MHWRender::MSelectionInfo& pickInfoArg
    ) : pickHit(pickHitArg), pickInfo(pickInfoArg) {}

    const HdxPickHit&                pickHit;
    const MHWRender::MSelectionInfo& pickInfo;
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
