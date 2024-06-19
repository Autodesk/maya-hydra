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
#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/usdImaging/usdImaging/sceneIndices.h>
#include <pxr/usdImaging/usdImaging/rootOverridesSceneIndex.h>

//The Pixar's namespace needs to be at the highest namespace level for TF_DECLARE_WEAK_AND_REF_PTRS to work.
PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

class DataProducerSceneIndexDataBase;//Predeclaration
TF_DECLARE_WEAK_AND_REF_PTRS(DataProducerSceneIndexDataBase);//Be able to use Ref counting pointers on DataProducerSceneIndexDataBase

/** DataProducerSceneIndexDataBase is storing information about a custom data producer scene index.
*   Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports in our records, we need ref counting.
*/
 class FVP_API DataProducerSceneIndexDataBase : public TfRefBase, public TfWeakBase
{
public:
    
    ///Structure passed as a parameter to create an instance of DataProducerSceneIndexDataBase
    struct CreationParameters
    {
        CreationParameters( const HdSceneIndexBaseRefPtr&   customDataProducerSceneIndex,
                            const std::string&              rendererNames,
                            const SdfPath&                  prefix,
                            void*                           dccNode) :
            _customDataProducerSceneIndex(customDataProducerSceneIndex), _rendererNames(rendererNames),
            _prefix(prefix),_dccNode(dccNode)
        {}

        //See below for an explanation of these parameters
        const HdSceneIndexBaseRefPtr&   _customDataProducerSceneIndex;
        const std::string&              _rendererNames;
        const SdfPath&                  _prefix;
        void*                           _dccNode;
    };

    struct CreationParametersForUsdStage
    {
        CreationParametersForUsdStage(UsdImagingCreateSceneIndicesInfo& createInfo, HdSceneIndexBaseRefPtr& finalSceneIndex, 
                                      UsdImagingStageSceneIndexRefPtr& stageSceneIndex, const SdfPath& prefix, void* dccNode)
            : _createInfo(createInfo), _finalSceneIndex(finalSceneIndex), _stageSceneIndex(stageSceneIndex), _prefix(prefix),_dccNode(dccNode)
        {}

        //See below for an explanation of these parameters
        UsdImagingCreateSceneIndicesInfo& _createInfo;
        HdSceneIndexBaseRefPtr& _finalSceneIndex;
        UsdImagingStageSceneIndexRefPtr& _stageSceneIndex;
        const SdfPath& _prefix;
        void* _dccNode;
    };

    ~DataProducerSceneIndexDataBase() override;
    
    //Used to set the usd stage scene indices
    void SetDataProducerSceneIndex(const HdSceneIndexBaseRefPtr& sceneIndex) {_dataProducerSceneIndex = sceneIndex;}
    void SetDataProducerLastSceneIndexChain(const HdSceneIndexBaseRefPtr& sceneIndex) {_lastSceneIndexChain = sceneIndex;}

    const HdSceneIndexBaseRefPtr&   GetDataProducerSceneIndex() const {return _dataProducerSceneIndex;}
    const HdSceneIndexBaseRefPtr&   GetDataProducerLastSceneIndexChain() const {return _lastSceneIndexChain;}
    const SdfPath&                  GetPrefix()const{return _prefix;}
    const std::string&              GetRendererNames() const {return _rendererNames;}

    /// Provide the node name from the DCC to be overriden in a DCC specific subclass
    virtual std::string GetDCCNodeName() const {return "";}

    virtual bool UpdateVisibility() = 0;
    virtual bool UpdateTransform() = 0;

protected:

    void _CreateSceneIndexChainForDataProducerSceneIndex();
    void _CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex);
    void _CreateSceneIndexChainForDataProducerSceneIndexWithoutDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex);
    void _CreateSceneIndexChainForUsdStageSceneIndex(CreationParametersForUsdStage& params);

    //Callback for UsdImagingCreateSceneIndicesInfo.OverridesSceneIndexCallback
    //Set the overridesSceneIndexCallback to insert our scene indices chain after the stage scene index and before the flatten scene index
    //If we don't do so, we cannot add a parent which will apply its matrix to the children because of the flatten scene index in the usd stage chain.
    PXR_NS::HdSceneIndexBaseRefPtr _CreateUsdStageSceneIndexChain(PXR_NS::HdSceneIndexBaseRefPtr const & inputStageSceneIndex);
    DataProducerSceneIndexDataBase(const CreationParameters& params);
    DataProducerSceneIndexDataBase(const CreationParametersForUsdStage& params);

    /// data producer scene index
    HdSceneIndexBaseRefPtr              _dataProducerSceneIndex = nullptr;
    /// data producer scene index rootPath for insertion (used in HdRenderIndex::InsertSceneIndex)
    SdfPath                             _prefix;
    /// Are the Hydra renderer(s) to which this scene index should be applied (e.g : "GL, Arnold") or DataProducerSceneIndexInterface::allViewports to apply to all viewports
    std::string                         _rendererNames;
    /// Is the DCC node so a MObject* DAG node for Maya
    void*                               _dccNode;

    //The following members are optional and only used when a dccNode was passed in the constructor
    
    /// Is the last scene index of the scene index chain when a dccNode was passed.
    HdSceneIndexBaseRefPtr              _lastSceneIndexChain = nullptr;
    
    /// _rootOverridesSceneIndex is used to set a transform and visibility at the root of the Usd stage/data producer scene index. It is used only when a dccNode was passed.
    /// With this scene index when you select the proxyshape node and move it or hide it, we apply the same operation on the stage, same for a data producer scene index with a hosting node.
    UsdImagingRootOverridesSceneIndexRefPtr _rootOverridesSceneIndex = nullptr;
};

}//End of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H

