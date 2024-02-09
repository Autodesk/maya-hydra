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

/* DataProducerSceneIndexDataBase is storing information about a custom data producer scene index.
*  Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports, we need ref counting.
*/

//Local headers
#include "fvpDataProducerSceneIndexDataBase.h"
#include "flowViewport/API/fvpViewportAPITokens.h"

//Hydra headers
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/imaging/hd/flatteningSceneIndex.h>
#include <pxr/imaging/hd/flattenedDataSourceProviders.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/flattenedVisibilityDataSourceProvider.h>
#include <pxr/imaging/hd/flattenedXformDataSourceProvider.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

/* DataProducerSceneIndexDataBase is storing information about a custom data producer scene index which is a scene index that create new primitives.
*  Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports in our records, we need ref counting.
*/
DataProducerSceneIndexDataBase::DataProducerSceneIndexDataBase(const CreationParameters& params)
{
    _dataProducerSceneIndex     = params._customDataProducerSceneIndex;
    _prefix                     = params._prefix;
    _lastSceneIndexChain        = params._customDataProducerSceneIndex;
    _rendererNames              = params._rendererNames;
    _dccNode                    = params._dccNode;
}

//For Usd stages
DataProducerSceneIndexDataBase::DataProducerSceneIndexDataBase(const CreationParametersForUsdStage& params)
{
    _dataProducerSceneIndex     = nullptr;//Will be set later
    _prefix                     = params._prefix;
    _lastSceneIndexChain        = nullptr;//Will be set later
    _rendererNames              = FvpViewportAPITokens->allRenderers;
    _dccNode                    = params._dccNode;
}

void DataProducerSceneIndexDataBase::UpdateHydraTransformFromParentPath() 
{ 
    if (! _rootOverridesSceneIndex){
        return;
    }

    _rootOverridesSceneIndex->SetRootTransform(_parentMatrix);
}

void DataProducerSceneIndexDataBase::UpdateVisibilityFromDCCNode(bool isVisible) 
{ 
    if (! _rootOverridesSceneIndex){
        return;
    }

    _rootOverridesSceneIndex->SetRootVisibility(isVisible);
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndex()
{
    if (_dccNode){
        _CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(_dataProducerSceneIndex);
        //Add a flattening scene index so that the root override scene index is applied (flatten only visibility and xform)
        using namespace HdMakeDataSourceContainingFlattenedDataSourceProvider;
        static HdContainerDataSourceHandle const flattenDataSource =
            HdRetainedContainerDataSource::New(
                HdVisibilitySchema::GetSchemaToken(),
                Make<HdFlattenedVisibilityDataSourceProvider>(),
                HdXformSchema::GetSchemaToken(),
                Make<HdFlattenedXformDataSourceProvider>());
        _lastSceneIndexChain = HdFlatteningSceneIndex::New(_lastSceneIndexChain, flattenDataSource);
    }
    else{
        _CreateSceneIndexChainForDataProducerSceneIndexWithoutDCCNode(_dataProducerSceneIndex);
    }
}

//Callback for UsdImagingCreateSceneIndicesInfo.OverridesSceneIndexCallback as we want to insert scene indices 
// before the flattening scene index from the usd stage scene indices
HdSceneIndexBaseRefPtr DataProducerSceneIndexDataBase::_CreateUsdStageSceneIndexChain(HdSceneIndexBaseRefPtr const & inputStageSceneIndex)
{
    _CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(inputStageSceneIndex);
    UpdateHydraTransformFromParentPath();//Update the transform, this is useful when deleting the node and undoing it
    return _lastSceneIndexChain;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForUsdStageSceneIndex(CreationParametersForUsdStage& params)
{
    if (params._dccNode){
        //Set the overridesSceneIndexCallback to insert our scene indices chain after the stage scene index and before the flatten scene index
        //If we don't do so, we cannot modify the transform or visibility to the children because of the flatten scene index in the usd stage chain.
        params._createInfo.overridesSceneIndexCallback = std::bind(&DataProducerSceneIndexDataBase::_CreateUsdStageSceneIndexChain, this, std::placeholders::_1);
    }

    //Create the scene indices chain
    UsdImagingSceneIndices sceneIndices = UsdImagingCreateSceneIndices(params._createInfo);
    params._finalSceneIndex = sceneIndices.finalSceneIndex;
    params._stageSceneIndex = sceneIndices.stageSceneIndex;
    
    _lastSceneIndexChain = sceneIndices.finalSceneIndex;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex)
{
    _rootOverridesSceneIndex    = UsdImagingRootOverridesSceneIndex::New(inputSceneIndex);
    _lastSceneIndexChain        = _rootOverridesSceneIndex;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndexWithoutDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex)
{
    HdSceneIndexBaseRefPtr sceneIndex = inputSceneIndex;
    if ( (!_prefix.IsEmpty()) && (_prefix != SdfPath::AbsoluteRootPath()) ){
        //Add a prefixing scene index to inputSceneIndex
        sceneIndex = HdPrefixingSceneIndex::New(sceneIndex, _prefix);
    }
    
    _lastSceneIndexChain = sceneIndex;
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE