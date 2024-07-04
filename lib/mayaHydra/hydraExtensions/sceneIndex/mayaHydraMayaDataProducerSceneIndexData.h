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

// UFE headers
#include "ufeExtensions/Global.h"
#include <ufe/hierarchy.h>
#include <ufe/object3d.h>
#include <ufe/scene.h>
#include <ufe/sceneNotification.h>
#include <ufe/transform3d.h>

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
    
    ///Is the way to get access to a new instance of MayaDataProducerSceneIndexData using ref counting for Usd Stages
    static TfRefPtr<MayaDataProducerSceneIndexData> New(FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params) {
        return TfCreateRefPtr(new MayaDataProducerSceneIndexData(params));
    }

    ///Destructor
    ~MayaDataProducerSceneIndexData() override;
    
    bool UpdateVisibility() override;
    bool UpdateTransform() override;

private:
    MayaDataProducerSceneIndexData(const FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params);
    MayaDataProducerSceneIndexData(FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params);
    
    void SetupUfeObservation(void* dccNode);

    // Path to the scene item, if it was added as one
    std::optional<Ufe::Path> _path;

    // To observe scene changes to the data producer scene item, if it exists
    Ufe::Observer::Ptr       _ufeSceneChangesHandler;

    class UfeSceneChangesHandler;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif //MAYA_HYDRA_HYDRAEXTENSIONS_SCENEINDEX_MAYA_DATA_PRODUCER_SCENE_INDEX_DATA_H

