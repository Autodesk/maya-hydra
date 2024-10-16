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
#include "mhLeadObjectPathTracker.h"

//Flow viewport headers
#include <flowViewport/colorPreferences/fvpColorPreferences.h>
#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>
#include <flowViewport/selection/fvpSelection.h>

//ufe
#include <ufe/globalSelection.h>
#include <ufe/observableSelection.h>
#include <ufe/selectionNotification.h>
#include <ufe/sceneItem.h>

PXR_NAMESPACE_USING_DIRECTIVE

// The MhLeadObjectPathTracker class is responsible for maintaining the lead object prim path and also notifying the old
// lead object when a new lead object is selected

namespace {

//GlobalSelectionChangedObs handles notifications from ufe when the global selection changes to track the lead object
class GlobalSelectionChangedObs : public Ufe::Observer
    {
        public:
            GlobalSelectionChangedObs(MayaHydra::MhLeadObjectPathTracker& leadObjectPathInstance)
            : _leadObjectPathTracker(leadObjectPathInstance)
            {
            }

        private:
            void operator()(const Ufe::Notification& notification) override
            {
                //selection has changed.
                
                //Update the lead object if it has changed
                auto globalSelection = Ufe::GlobalSelection::get();
                if (globalSelection->size() > 0) {
                    auto newLeadObjectUfePath = globalSelection->back()->path();
                    auto currentLeadObjectUfePath = _leadObjectPathTracker.getLeadObjectUfePath();
                    if (newLeadObjectUfePath != currentLeadObjectUfePath){
                        _leadObjectPathTracker.setLeadObjectUfePath(newLeadObjectUfePath);
                    }
                }else {
                    _leadObjectPathTracker.setLeadObjectUfePath({});
                }
            }

            MayaHydra::MhLeadObjectPathTracker& _leadObjectPathTracker;
    };

}
namespace MAYAHYDRA_NS_DEF {

MhLeadObjectPathTracker::MhLeadObjectPathTracker(const HdSceneIndexBaseRefPtr& sceneIndexWithPathInterface, 
                                            MhDirtyLeadObjectSceneIndexRefPtr& dirtyLeadObjectSceneIndex) 
    : _pathInterface(dynamic_cast<const Fvp::PathInterface*>(&*sceneIndexWithPathInterface))
    , _ufeSelectionObserver (std::make_shared<GlobalSelectionChangedObs>(*this))
    , _dirtyLeadObjectSceneIndex(dirtyLeadObjectSceneIndex)
{
    TF_AXIOM(_pathInterface);

    const Ufe::GlobalSelection::Ptr& ufeSelection = Ufe::GlobalSelection::get();
    if (ufeSelection->size() > 0){
        const Ufe::Selection& selection = *(ufeSelection);
        auto leadObjectSceneItem = selection.back();//get last selected
        _leadObjectUfePath = leadObjectSceneItem->path();
        //_leadObjectPrimPaths can be empty with a valid _leadObjectUfePath when the lead object is in a data producer scene index not yet added to the merging scene index
        //This is fixed at some point by calling updatePrimPaths()
        _leadObjectPrimPaths = _pathInterface->SceneIndexPaths(_leadObjectUfePath);
    }

   // Add ourself as an observer to the selection
   
   ufeSelection->addObserver(_ufeSelectionObserver);
}

MhLeadObjectPathTracker::~MhLeadObjectPathTracker()
{
    Ufe::GlobalSelection::get()->removeObserver(_ufeSelectionObserver);
    _ufeSelectionObserver = nullptr;
}

bool MhLeadObjectPathTracker::isLeadObjectPrim(const PXR_NS::SdfPath& primPath) const
{
    //_leadObjectPrimPaths can be hierarchy paths, so we need to check if the primPath is a prefix
    //of a lead object prim path
    for (const auto& leadObjectPrimPath : _leadObjectPrimPaths) {
        if (primPath.HasPrefix(leadObjectPrimPath)) {
            return true;
        }
    }
    return false;
}

void MhLeadObjectPathTracker::setLeadObjectUfePath(const Ufe::Path& newLeadObjectUfePath)
{
    //newLeadObjectUfePath be an empty Ufe::Path
    if (_leadObjectUfePath == newLeadObjectUfePath) {
       return;
    }
    
    auto oldLeadObjectPrimPaths = _leadObjectPrimPaths;

    _leadObjectUfePath  = newLeadObjectUfePath;
    _leadObjectPrimPaths = _pathInterface->SceneIndexPaths(_leadObjectUfePath);

    // Dirty the previous lead object
    if(_dirtyLeadObjectSceneIndex){
        _dirtyLeadObjectSceneIndex->dirtyLeadObjectRelatedPrims(oldLeadObjectPrimPaths, _leadObjectPrimPaths);
    }
}

void MhLeadObjectPathTracker::updatePrimPaths() 
{ 
   // Update the lead object prim paths in case it was not valid yet
    if ( (_leadObjectUfePath.size() > 0) && _leadObjectPrimPaths.empty()) {
        _leadObjectPrimPaths = _pathInterface->SceneIndexPaths(_leadObjectUfePath);
    }
}

}//End of MAYAHYDRA_NS_DEF