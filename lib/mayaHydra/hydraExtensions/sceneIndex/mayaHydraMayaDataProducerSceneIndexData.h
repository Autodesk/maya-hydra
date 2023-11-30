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
#ifndef MAYA_HYDRA_HYDRAEXTENSIONS_SCENEINDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_DATA_H
#define MAYA_HYDRA_HYDRAEXTENSIONS_SCENEINDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_DATA_H

//Flow viewport headers
#include <flowViewport/API/perViewportSceneIndicesData/fvpDataProducerSceneIndexDataBase.h>

//Maya headers
#include <maya/MDagPath.h>
#include <maya/MObjectHandle.h>
#include <maya/MNodeMessage.h>
#include <maya/MCallbackIdArray.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaDataProducerSceneIndexData;
TF_DECLARE_WEAK_AND_REF_PTRS(MayaDataProducerSceneIndexData);//Be able to use Ref counting pointers on MayaDataProducerSceneIndexData

/** MayaDataProducerSceneIndexData is storing information about a custom data producer scene index.
*   Since an instance of the MayaDataProducerSceneIndexData class can be shared between multiple viewports in our records, we need ref counting.
*/
 class MayaDataProducerSceneIndexData : public FVP_NS_DEF::DataProducerSceneIndexDataBase
{
public:
    
    ///Is the way to get access to a new instance of MayaDataProducerSceneIndexData using ref counting
    static TfRefPtr<MayaDataProducerSceneIndexData> New(const FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params) {
        return TfCreateRefPtr(new MayaDataProducerSceneIndexData(params));
    }
    
    ///Destructor
    ~MayaDataProducerSceneIndexData() override;
    
    /// Get the MObject handle
    const MObjectHandle& getObjHandle()const {return _mObjHandle;}  

    /// Provide the node name from maya
    std::string GetDCCNodeName() const override;

    ///Update transform from maya node
    void UpdateTransformFromMayaNode();
   
private:
    MayaDataProducerSceneIndexData(const FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params);

    //The following members are optional and used only when a dccNode was passed in the constructor of DataProducerSceneIndexDataBase
    
    /// Is the MObjectHandle of the maya node shape, it may be invalid if no maya node MObject pointer was passed.
    MObjectHandle                       _mObjHandle;
    /// Is the dag path of the Maya node passed in AppendSceneIndex. It is used only when a dccNode was passed.
    MDagPath                            _mayaNodeDagPath;
    /// Are the callbacks Ids set in maya to forward the changes done in maya on the data producer scene index.
    MCallbackIdArray                    _nodeMessageCallbackIds;
    /// Are the callbacks Ids set in maya to handle delete and deletion undo/redo
    MCallbackIdArray                    _dGMessageCallbackIds;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //MAYA_HYDRA_HYDRAEXTENSIONS_SCENEINDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_DATA_H

