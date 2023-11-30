//
// Copyright 2023 Autodesk
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
#ifndef MAYAHYDRA_SCENE_INDEX_MAYA_FILTERING_SCENE_INDEX_DATA_H
#define MAYAHYDRA_SCENE_INDEX_MAYA_FILTERING_SCENE_INDEX_DATA_H

//Flow Viewport headers
#include <flowViewport/API/fvpFilteringSceneIndexClientFwd.h>
#include <flowViewport/API/fvpFilteringSceneIndexInterface.h>
#include <flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndexDataBase.h>

//Maya headers
#include <maya/MObjectHandle.h>
#include <maya/MNodeMessage.h>
#include <maya/MCallbackIdArray.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaFilteringSceneIndexData;
TF_DECLARE_REF_PTRS(MayaFilteringSceneIndexData);//Be able to use Ref counting pointers on MayaFilteringSceneIndexData

/**This class is a Maya implementation of FilteringSceneIndexDataBase with specific variables and callbacks for Maya since FilteringSceneIndexDataBase is
* part of Flow viewport which is DCC agnostic.
*/
 class MayaFilteringSceneIndexData : public FVP_NS_DEF::FilteringSceneIndexDataBase
{
public:
    static TfRefPtr<MayaFilteringSceneIndexData> New(const std::shared_ptr<::FVP_NS_DEF::FilteringSceneIndexClient>& client) {
        return TfCreateRefPtr(new MayaFilteringSceneIndexData(client));
    }
    
    ~MayaFilteringSceneIndexData() override;
    
    ///The following members are optional and used only when a dccNode was passed in the FilteringSceneIndexClient
    
    /// Is the MObjectHandle of the maya node shape, it may be invalid if no maya node MObject pointer was passed in the FilteringSceneIndexClient.
    MObjectHandle                       _mObjHandle;  
    
    /// Are the callbacks Ids set in maya to forward the changes done in maya on the dataProducer scene index.
    MCallbackIdArray                    _nodeMessageCallbackIds;

    /// Are the callbacks Ids set in maya to handle delete and deletion undo/redo
    MCallbackIdArray                    _dGMessageCallbackIds;

private:
    MayaFilteringSceneIndexData(const std::shared_ptr<::FVP_NS_DEF::FilteringSceneIndexClient>& client);
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //MAYAHYDRA_SCENE_INDEX_MAYA_FILTERING_SCENE_INDEX_DATA_H

