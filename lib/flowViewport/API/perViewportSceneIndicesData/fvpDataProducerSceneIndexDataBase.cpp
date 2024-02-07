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
#include "flowViewport/sceneIndex/fvpParentDataModifierSceneIndex.h"
#include "flowViewport/sceneIndex/fvpPathInterfaceSceneIndex.h"
#include "flowViewport/API/fvpViewportAPITokens.h"

#ifdef CODE_COVERAGE_WORKAROUND
#include <flowViewport/fvpUtils.h>
#endif

//Hydra headers
#include <pxr/imaging/hd/dirtyBitsTranslator.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/flatteningSceneIndex.h>
#include <pxr/imaging/hd/prefixingSceneIndex.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>
#include <pxr/imaging/hd/flattenedVisibilityDataSourceProvider.h>
#include <pxr/imaging/hd/flattenedXformDataSourceProvider.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/flattenedDataSourceProviders.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

/* DataProducerSceneIndexDataBase is storing information about a custom data producer scene index which is a scene index that create new primitives.
*  Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports in our records, we need ref counting.
*/
DataProducerSceneIndexDataBase::DataProducerSceneIndexDataBase(const CreationParameters& params)
{
    _parentMatrix.SetIdentity();

    _dataProducerSceneIndex     = params._customDataProducerSceneIndex;
    _prefix                     = params._prefix;
    _lastSceneIndexChain        = params._customDataProducerSceneIndex;
    _rendererNames              = params._rendererNames;
    _dccNode                    = params._dccNode;
}

//For Usd stages
DataProducerSceneIndexDataBase::DataProducerSceneIndexDataBase(const CreationParametersForUsdStage& params)
{
    _parentMatrix.SetIdentity();

    _dataProducerSceneIndex     = nullptr;//Will be set later
    _prefix                     = params._prefix;
    _lastSceneIndexChain        = nullptr;//Will be set later
    _rendererNames              = FvpViewportAPITokens->allRenderers;
    _dccNode                    = params._dccNode;
}

DataProducerSceneIndexDataBase::~DataProducerSceneIndexDataBase()
{
#ifdef CODE_COVERAGE_WORKAROUND
    Fvp::leakSceneIndex(_parentDataModifierSceneIndex);
#endif
}

void DataProducerSceneIndexDataBase::UpdateHydraTransformFromParentPath() 
{ 
    if (! _parentDataModifierSceneIndex){
        return;
    }

    //Update the matrix in the filtering scene index
    _parentDataModifierSceneIndex->SetParentTransformMatrix(_parentMatrix);
    
    //This is still a WIP trying to dirty the prims correctly...
    /*HdDataSourceLocatorSet locators;
    locators.append(HdXformSchema::GetDefaultLocator());
    locators.append(HdVisibilitySchema::GetDefaultLocator());
    locators.append(HdPrimvarsSchema::GetDefaultLocator());
    _retainedSceneIndex->DirtyPrims({{ _parentPath, locators}});
    */
    
     //Set the transform prim as dirty so it gets recomputed
    /*HdDataSourceLocatorSet locators;
    HdDirtyBitsTranslator::RprimDirtyBitsToLocatorSet(HdTokens->transform, HdChangeTracker::AllSceneDirtyBits, &locators);
    
    //Set dirty this field in the retained scene index for the parent prim
    //_retainedSceneIndex->DirtyPrims( { {_parentPath, locators} } );
    */
    
    //Update by removing the prim and adding it again
    RemoveParentPrimFromSceneIndex();
    AddParentPrimToSceneIndex();
}

void DataProducerSceneIndexDataBase::UpdateVisibilityFromDCCNode(bool isVisible) 
{ 
    if (! _parentDataModifierSceneIndex){
        return;
    }
    //Update the visibility in the filtering scene index
    _parentDataModifierSceneIndex->SetParentVisibility(isVisible);
    
    //This is still a WIP trying to dirty the prims correctly...
    /* //Set the transform prim as dirty so it gets recomputed
    HdDataSourceLocatorSet locators;
    HdDirtyBitsTranslator::RprimDirtyBitsToLocatorSet(HdTokens->transform, HdChangeTracker::DirtyVisibility, &locators);
    
    //Set dirty this field in the retained scene index for the parent prim
    //_retainedSceneIndex->DirtyPrims({{_parentPath, locators}});
    */
    
    //Update by removing the prim and adding it again
    RemoveParentPrimFromSceneIndex();
    AddParentPrimToSceneIndex();
}

void DataProducerSceneIndexDataBase::AddParentPrimToSceneIndex()
{
    if(_visible){
        return;
    }

    //Arrays of added prims
    HdRetainedSceneIndex::AddedPrimEntries  addedPrims;

    //We are creating a XForm prim which has only 2 attributes a matrix and a visibility.
    //This prim will be the parent of all data producer scene index primitives so we can change their transform or visibility from the parent
    HdRetainedSceneIndex::AddedPrimEntry parentPrimEntry;
    parentPrimEntry.primPath      = _prefix;
    parentPrimEntry.primType      = HdTokens->transform;
    parentPrimEntry.dataSource    = HdRetainedContainerDataSource::New(
                            HdXformSchemaTokens->xform,
                            HdXformSchema::Builder().SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(_parentMatrix)).Build(),

                            HdVisibilitySchemaTokens->visibility,
                            HdVisibilitySchema::BuildRetained(HdRetainedTypedSampledDataSource<bool>::New(true))
    );
                    
    addedPrims.emplace_back(parentPrimEntry);
    
    
    //Add new prims to the scene index
    _retainedSceneIndex->AddPrims(addedPrims);
    _visible = true;
}

void DataProducerSceneIndexDataBase::RemoveParentPrimFromSceneIndex()
{
    if(!_visible){
        return;
    }

    _retainedSceneIndex->RemovePrims({ _prefix});
    _visible = false;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndex()
{
    if (_dccNode){
        _CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(_dataProducerSceneIndex);
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
    return _lastSceneIndexChain;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForUsdStageSceneIndex(CreationParametersForUsdStage& params)
{
    //Set the overridesSceneIndexCallback to insert our scene indices chain after the stage scene index and before the flatten scene index
    //If we don't do so, we cannot add a parent which will apply its matrix to the children because of the flatten scene index in the usd stage chain.
    params._createInfo.overridesSceneIndexCallback = std::bind(&DataProducerSceneIndexDataBase::_CreateUsdStageSceneIndexChain, this, std::placeholders::_1);

    //Create the scene indices chain
    UsdImagingSceneIndices sceneIndices = UsdImagingCreateSceneIndices(params._createInfo);
    params._finalSceneIndex = sceneIndices.finalSceneIndex;
    params._stageSceneIndex = sceneIndices.stageSceneIndex;
    
    _lastSceneIndexChain = sceneIndices.finalSceneIndex;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndexWithDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex)
{
    //Create a parent path to parent the whole inputSceneIndex prims, try to use the DCC node name
    std::string nodeName = GetDCCNodeName();

    if (nodeName.empty()){
        _prefix = _prefix.AppendPath(SdfPath(TfStringPrintf("DataProducerSI_%p", &(*inputSceneIndex))));
    }else{
        //A nodeName was provided by the DCC implementation, and it was sanitized for Hydra
        _prefix = _prefix.AppendPath(SdfPath(nodeName));
    }
    
    //Create a retainedsceneindex to inject a parent path to the be the parent of inputSceneIndex
    _retainedSceneIndex = HdRetainedSceneIndex::New();
    // Add a prim inside which will be the parent of inputSceneIndex, its SdfPath will updated in _inOutData._parentPath
    AddParentPrimToSceneIndex();

    //Create a filtering scene index to update the information (transform, visibility,...) from the parent prim.
    _parentDataModifierSceneIndex = ParentDataModifierSceneIndex::New(_retainedSceneIndex, _prefix, _parentMatrix, true);
    
    //Add a prefixing scene index to inputSceneIndex to set the parent which we added to the retainedsceneindex
    HdPrefixingSceneIndexRefPtr prefixingSceneIndex = HdPrefixingSceneIndex::New(inputSceneIndex, _prefix);
    
    //Use a merging scene index to merge the prefixing and the retainedsceneindex
    HdMergingSceneIndexRefPtr mergingSceneIndex = HdMergingSceneIndex::New();
    mergingSceneIndex->AddInputScene(_parentDataModifierSceneIndex, SdfPath::AbsoluteRootPath());
    mergingSceneIndex->AddInputScene(prefixingSceneIndex, SdfPath::AbsoluteRootPath());
    
    //Add a flattening scene index to the merging scene index, flatten only transform and visibility
    static HdContainerDataSourceHandle const flattenedTransformAndVisibilityDataSourceHandle =
            HdRetainedContainerDataSource::New(
                HdVisibilitySchema::GetSchemaToken(),
                HdMakeDataSourceContainingFlattenedDataSourceProvider::Make<HdFlattenedVisibilityDataSourceProvider>(),
                HdXformSchema::GetSchemaToken(),
                HdMakeDataSourceContainingFlattenedDataSourceProvider::Make<HdFlattenedXformDataSourceProvider>()
            );

    _lastSceneIndexChain = HdFlatteningSceneIndex::New(mergingSceneIndex, flattenedTransformAndVisibilityDataSourceHandle);//Flattening scene index to apply transform/visibility on children
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndexWithoutDCCNode(HdSceneIndexBaseRefPtr const & inputSceneIndex)
{
    HdSceneIndexBaseRefPtr sceneIndex = inputSceneIndex;
    if ( (!_prefix.IsEmpty()) && (_prefix != SdfPath::AbsoluteRootPath()) ){
        //Add a prefixing scene index to inputSceneIndex
        sceneIndex = HdPrefixingSceneIndex::New(sceneIndex, _prefix);
    }
    
    //Add a PathInterfaceSceneIndex for selection
    _lastSceneIndexChain = sceneIndex;
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE
