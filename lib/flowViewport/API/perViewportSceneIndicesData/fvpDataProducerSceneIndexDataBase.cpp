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

PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

/* DataProducerSceneIndexDataBase is storing information about a custom data producer scene index which is a scene index that create new primitives.
*  Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports in our records, we need ref counting.
*/
DataProducerSceneIndexDataBase::DataProducerSceneIndexDataBase(const CreationParameters& params)
{
    _parentMatrix.SetIdentity();

    _dataProducerSceneIndex                             = params._customDataProducerSceneIndex;
    _customDataProducerSceneIndexRootPathForInsertion   = params._customDataProducerSceneIndexRootPathForInsertion;
    _lastSceneIndexChain                                = params._customDataProducerSceneIndex;
    _rendererNames                                      = params._rendererNames;
    _dccNode                                            = params._dccNode;
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
    parentPrimEntry.primPath      = _parentPath;
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

    _retainedSceneIndex->RemovePrims({ _parentPath});
    _visible = false;
}

void DataProducerSceneIndexDataBase::_CreateSceneIndexChainForDataProducerSceneIndex()
{
    //Create a parent path to parent the whole _dataProducerSceneIndex prims, try to use the DCC node name
    std::string nodeName = GetDCCNodeName();
    if (nodeName.empty()){
        _parentPath = SdfPath(TfStringPrintf("/DataProducerSI_%p", &(*_dataProducerSceneIndex)));
    }else{
        //A nodeName was provided by the DCC implementation, and it was sanitized for Hydra
        _parentPath = SdfPath(std::string("/")+nodeName);
    }
    
    //Create a retainedsceneindex to inject a parent path to the be the parent of _dataProducerSceneIndex
    _retainedSceneIndex = HdRetainedSceneIndex::New();
    // Add a prim inside which will be the parent of _dataProducerSceneIndex, its SdfPath will updated in _inOutData._parentPath
    AddParentPrimToSceneIndex();

    //Create a filtering scene index to update the information (transform, visibility,...) from the parent prim.
    _parentDataModifierSceneIndex = ParentDataModifierSceneIndex::New(_retainedSceneIndex, _parentPath, _parentMatrix, true);
    
    //Add a prefixing scene index to _dataProducerSceneIndex to set the parent which we added to the retainedsceneindex
    HdPrefixingSceneIndexRefPtr prefixingSceneIndex = HdPrefixingSceneIndex::New(_dataProducerSceneIndex, _parentPath);
    
    //Use a merging scene index to merge the prefixing and the retainedsceneindex
    HdMergingSceneIndexRefPtr mergingSceneIndex = HdMergingSceneIndex::New();
    mergingSceneIndex->AddInputScene(_parentDataModifierSceneIndex, SdfPath::AbsoluteRootPath());
    mergingSceneIndex->AddInputScene(prefixingSceneIndex, SdfPath::AbsoluteRootPath());
    
    //Add a flattening scene index to the merging scene index, flatten only transform and visibility, meaning that transform and visibility should not have been already flatten previously !
    static HdContainerDataSourceHandle const flattenedTransformAndVisibilityDataSourceHandle =
            HdRetainedContainerDataSource::New(
                HdVisibilitySchema::GetSchemaToken(),
                HdMakeDataSourceContainingFlattenedDataSourceProvider::Make<HdFlattenedVisibilityDataSourceProvider>(),
                HdXformSchema::GetSchemaToken(),
                HdMakeDataSourceContainingFlattenedDataSourceProvider::Make<HdFlattenedXformDataSourceProvider>()
            );

    _lastSceneIndexChain = HdFlatteningSceneIndex::New(mergingSceneIndex, flattenedTransformAndVisibilityDataSourceHandle);//Flatten
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE