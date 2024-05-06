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

namespace MAYAHYDRA_NS_DEF {

MhWireframeColorInterfaceImp::MhWireframeColorInterfaceImp(const std::shared_ptr<Fvp::Selection>& selection
                                                         , const std::shared_ptr<MhLeadObjectPathTracker>& leadObjectPathTracker) 
    : _selection(selection)
    , _leadObjectPathTracker(leadObjectPathTracker)
{ 
    TF_AXIOM(_selection);
    Fvp::ColorPreferences::getInstance().getColor(FvpColorPreferencesTokens->wireframeSelection, _leadWireframeColor);
    Fvp::ColorPreferences::getInstance().getColor(FvpColorPreferencesTokens->wireframeSelectionSecondary, _activeWireframeColor);
    Fvp::ColorPreferences::getInstance().getColor(FvpColorPreferencesTokens->polymeshDormant, _dormantWireframeColor);
}

bool MhWireframeColorInterfaceImp::_isSelected(const SdfPath& primPath, bool& outIsTheLastSelected)const
{
    outIsTheLastSelected = false;

    const bool selected = _selection->HasFullySelectedAncestorInclusive(primPath);
    if (selected){
        //Update outIsTheLastSelected
        outIsTheLastSelected = _leadObjectPathTracker->isLeadObject(primPath);
        return true;
    }
    
    return selected;
}

GfVec4f MhWireframeColorInterfaceImp::getWireframeColor(const SdfPath& primPath) const { 
    bool isLastSelected = false;
    if (_isSelected(primPath, isLastSelected)){
        return isLastSelected ? _leadWireframeColor : _activeWireframeColor;
    }   

    return _dormantWireframeColor;
}

}//End of MAYAHYDRA_NS_DEF