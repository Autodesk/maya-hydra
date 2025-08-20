//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

//Local headers
#include "mhWireframeColorInterfaceImp.h"
#include "mixedUtils.h"

//Flow viewport headers
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <flowViewport/selection/fvpSelection.h>

//ufe
#include <ufe/globalSelection.h>
#include <ufe/selection.h>
#include <ufe/observableSelection.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MhWireframeColorInterfaceImp::MhWireframeColorInterfaceImp(const std::shared_ptr<Fvp::Selection>& selection
                                                         , const std::weak_ptr<MhLeadObjectPathTracker>& leadObjectPathTracker) 
    : _activeWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->wireframeSelectionSecondary))
    , _leadWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->wireframeSelection))
    , _dormantWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->polymeshDormant))
    , _selection(selection)
    , _leadObjectPathTracker(leadObjectPathTracker)
{ 
    TF_AXIOM(_selection);
}

MhWireframeColorInterfaceImp::SelectionState MhWireframeColorInterfaceImp::_getSelectionState(const PXR_NS::SdfPath& primPath) const
{
    if (_selection->HasFullySelectedAncestorInclusive(primPath)){
        auto pt = _leadObjectPathTracker.lock();
        if (!pt) {
            TF_WARN("Illegal access to path tracker in %s, wireframe color will be incorrect.", TF_FUNC_NAME().data());
            return kDormant;
        }
        return (pt->isLeadObject(primPath)) ? kLead : kActive;
    }
    
    return kDormant;
}

MhWireframeColorInterfaceImp::SelectionState MhWireframeColorInterfaceImp::_getSelectionState(const Fvp::PrimSelection& primSelection) const
{
    if (_selection->HasFullySelectedAncestorInclusive(primSelection.primPath)){
        auto pt = _leadObjectPathTracker.lock();
        if (!pt) {
            TF_WARN("Illegal access to path tracker in %s, wireframe color will be incorrect.", TF_FUNC_NAME().data());
            return kDormant;
        }
        return (pt->isLeadObject(primSelection)) ? kLead : kActive;
    }
    
    return kDormant;
}

GfVec4f MhWireframeColorInterfaceImp::_getWireframeColor(const SelectionState& selectionState) const
{
    switch (selectionState) {
        case kLead:
            return _leadWireframeColor;
        case kActive:
            return _activeWireframeColor;
        default:
            break;
    }

    return _dormantWireframeColor;
}

GfVec4f MhWireframeColorInterfaceImp::getWireframeColor(const SdfPath& primPath) const { 
    std::string pathString = primPath.GetString();
    
    // Dual-hierarchy colour assignment - filtering is done in PiInstancerWhSi
    if (pathString.find("Highlight_Lead") != std::string::npos) {
        return _getWireframeColor(kLead);
    }
    else if (pathString.find("Highlight_Active") != std::string::npos) {
        return _getWireframeColor(kActive);
    }

    return _getWireframeColor(_getSelectionState(primPath));
}

GfVec4f MhWireframeColorInterfaceImp::getWireframeColor(const Fvp::PrimSelection& primSelection) const { 
    return _getWireframeColor(_getSelectionState(primSelection));
}

}//End of MAYAHYDRA_NS_DEF
