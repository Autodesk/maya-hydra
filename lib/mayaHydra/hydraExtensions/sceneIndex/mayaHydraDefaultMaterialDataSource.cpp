//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#include "mayaHydraDefaultMaterialDataSource.h"

#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexUtils.h>

#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>

PXR_NAMESPACE_OPEN_SCOPE

// ----------------------------------------------------------------------------

MayaHydraDefaultMaterialDataSource::MayaHydraDefaultMaterialDataSource(
    const SdfPath& id,
    TfToken type,
    MayaHydraSceneIndex* sceneIndex)
    : _id(id)
    , _type(type)
    , _sceneIndex(sceneIndex)
{
}


TfTokenVector
MayaHydraDefaultMaterialDataSource::GetNames()
{
    TfTokenVector result;
    result.push_back(HdMaterialSchemaTokens->material);
    return result;
}

HdDataSourceBaseHandle
MayaHydraDefaultMaterialDataSource::Get(const TfToken& name)
{
    if (name == HdMaterialSchemaTokens->material) {
       return _GetMaterialDataSource();
    }
    else if (name ==
             HdMaterialBindingsSchema::GetSchemaToken()
             ) {
       return _GetMaterialBindingDataSource();
    }
    return nullptr;
}

HdDataSourceBaseHandle
MayaHydraDefaultMaterialDataSource::_GetMaterialBindingDataSource()
{
    const SdfPath path = _sceneIndex->GetMaterialId(_id);
    if (path.IsEmpty()) {
        return nullptr;
    }
    static const TfToken purposes[] = {
        HdMaterialBindingsSchemaTokens->allPurpose
    };
    HdDataSourceBaseHandle const materialBindingSources[] = {
        HdMaterialBindingSchema::Builder()
            .SetPath(
                HdRetainedTypedSampledDataSource<SdfPath>::New(path))
            .Build()
    };

    return
        HdMaterialBindingsSchema::BuildRetained(
            TfArraySize(purposes), purposes, materialBindingSources);
}

HdDataSourceBaseHandle
MayaHydraDefaultMaterialDataSource::_GetMaterialDataSource()
{    
    VtValue materialContainer = _sceneIndex->GetMaterialResource(_id);

    if (!materialContainer.IsHolding<HdMaterialNetworkMap>()) {
        return nullptr;
    }

    HdMaterialNetworkMap hdNetworkMap = 
        materialContainer.UncheckedGet<HdMaterialNetworkMap>();
    HdContainerDataSourceHandle materialDS = nullptr;    
    if (!_ConvertHdMaterialNetworkToHdDataSources(
        hdNetworkMap,
        &materialDS) ) {
        return nullptr;
    }
    return materialDS;
}

PXR_NAMESPACE_CLOSE_SCOPE
