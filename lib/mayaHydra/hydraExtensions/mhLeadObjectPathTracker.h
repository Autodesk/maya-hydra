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
#ifndef MAYA_HYDRA_LEAD_OBJECT_PATH_H
#define MAYA_HYDRA_LEAD_OBJECT_PATH_H

//Local headers
#include "mayaHydraLib/api.h"
#include "mayaHydraLib/mayaHydra.h"
#include "mayaHydraLib/sceneIndex/mhDirtyLeadObjectSceneIndex.h"

//Flow viewport headers
#include <flowViewport/sceneIndex/fvpPathInterface.h>

//ufe
#include <ufe/observer.h>
#include <ufe/sceneItem.h>

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

//Predeclaration
namespace MAYAHYDRA_NS_DEF {

/// \class MhLeadObjectPathTracker
/// This class is responsible for maintaining the lead object prim path and also notifying the old
/// lead object when a new lead object is selected
class MhLeadObjectPathTracker
{
public:
    MAYAHYDRALIB_API
    MhLeadObjectPathTracker(const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndexWithPathInterface, MhDirtyLeadObjectSceneIndexRefPtr& dirtyPreviousLeadObjectSceneIndex);

    MAYAHYDRALIB_API
    ~MhLeadObjectPathTracker();

    MAYAHYDRALIB_API
    bool isLeadObject(const PXR_NS::SdfPath& primPath) const;
    
    MAYAHYDRALIB_API
    Ufe::Path getLeadObjectUfePath() const {return _leadObjectUfePath;}

    MAYAHYDRALIB_API
    void setNewLeadObjectSceneItem(const Ufe::Path& newLeadObjectUfePath);

    MAYAHYDRALIB_API
    void ClearLeadObject();

    MAYAHYDRALIB_API
    void updateAfterDataProducerSceneIndicesLoaded(); // This is called after the data producer
                                                      // scene indices are loaded

private:
    const Fvp::PathInterface* const _pathInterface {nullptr};
    PXR_NS::SdfPath                 _leadObjectPrimPath;
    Ufe::Observer::Ptr              _ufeSelectionObserver {nullptr};
    Ufe::Path                       _leadObjectUfePath;
    MhDirtyLeadObjectSceneIndexRefPtr _dirtyLeadObjectSceneIndex;
};

}//end of namespace MAYAHYDRA_NS_DEF

#endif //MAYA_HYDRA_LEAD_OBJECT_PATH_H
