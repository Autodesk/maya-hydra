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

//Flow viewport headers
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <flowViewport/selection/fvpSelection.h>

//ufe
#include <ufe/globalSelection.h>
#include <ufe/selection.h>
#include <ufe/observableSelection.h>

PXR_NAMESPACE_USING_DIRECTIVE

// An implementation for maya of the WireframeColorInterface to get the wireframe color from a prim for selection highlighting
namespace {
    PXR_NS::GfVec4f getPreferencesColor(const PXR_NS::TfToken& token) {
        PXR_NS::GfVec4f color;
        Fvp::ColorPreferences::getInstance().getColor(token, color);
        return color;
    }
}
namespace MAYAHYDRA_NS_DEF {

MhWireframeColorInterfaceImp::MhWireframeColorInterfaceImp(const std::shared_ptr<Fvp::Selection>& selection
                                                         , const std::shared_ptr<MhLeadObjectPathTracker>& leadObjectPathTracker) 
    : _activeWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->wireframeSelectionSecondary))
    , _leadWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->wireframeSelection))
    , _dormantWireframeColor (getPreferencesColor(FvpColorPreferencesTokens->polymeshDormant))
    , _selection(selection)
    , _leadObjectPathTracker(leadObjectPathTracker)
{ 
    TF_AXIOM(_selection);
}

MhWireframeColorInterfaceImp::SelectionState MhWireframeColorInterfaceImp::_getSelectionState(const PXR_NS::SdfPath& primPath)const
{
    if (_selection->HasFullySelectedAncestorInclusive(primPath)){
        return (_leadObjectPathTracker->isLeadObjectPrim(primPath)) ? kLead : kActive;
    }
    
    return kDormant;
}

GfVec4f MhWireframeColorInterfaceImp::getWireframeColor(const SdfPath& primPath) const { 
    SelectionState selState = _getSelectionState(primPath);
    switch (selState) {
        case kLead:
            return _leadWireframeColor;
        case kActive:
            return _activeWireframeColor;
        default:
        break;
    }

    return _dormantWireframeColor;
}

}//End of MAYAHYDRA_NS_DEF