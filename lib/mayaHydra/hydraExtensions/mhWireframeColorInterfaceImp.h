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
#ifndef MAYA_HYDRA_WIREFRAME_COLOR_INTERFACE_IMP_H
#define MAYA_HYDRA_WIREFRAME_COLOR_INTERFACE_IMP_H

//Local headers
#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydra.h>
#include <mayaHydraLib/mhLeadObjectPathTracker.h>

//Flow viewport headers
#include <flowViewport/fvpWireframeColorInterface.h>
#include <flowViewport/selection/fvpSelectionFwd.h>
#include <flowViewport/sceneIndex/fvpPathInterface.h>

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace MAYAHYDRA_NS_DEF {

/// \class MhWireframeColorInterfaceImp
/// An implementation for maya of the WireframeColorInterface to get the wireframe color from a prim for selection highlighting
class MhWireframeColorInterfaceImp : public Fvp::WireframeColorInterface
{
public:
    MAYAHYDRALIB_API
    MhWireframeColorInterfaceImp(const std::shared_ptr<Fvp::Selection>& selection, const std::shared_ptr<MhLeadObjectPathTracker>& _leadObjectPathTracker);

    //Get the wireframe color of a primitive for selection highlighting, 
    // this checks if the prim is selected or not and if it is selected, 
    // it returns the maya lead or active color depending on the lead of the selection
    MAYAHYDRALIB_API
    PXR_NS::GfVec4f getWireframeColor(const PXR_NS::SdfPath& primPath) const override;

private:
    enum SelectionState {kLead, kActive, kDormant};

    SelectionState _getSelectionState(const PXR_NS::SdfPath& primPath)const;

    //Colors used by wireframe selection highlighting
    PXR_NS::GfVec4f _activeWireframeColor;
    PXR_NS::GfVec4f _leadWireframeColor;
    PXR_NS::GfVec4f _dormantWireframeColor;

    const Fvp::SelectionPtr    _selection;
    const std::shared_ptr<MhLeadObjectPathTracker> _leadObjectPathTracker;
};

}//end of namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_WIREFRAME_COLOR_INTERFACE_IMP_H
