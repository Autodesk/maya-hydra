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

//We use a this filtering scene index to update the data from a HdRetainedSceneIndex where we inserted a parent prim to be the parent of all prims from an dataProducer scene index
// which is hosted in a DCC node.
// We only update the data from the parent SdfPath, it has the same transform as the DCC node which contains the data dataProducer scene index.

//Local headers
#include "fvpParentDataModifierSceneIndex.h"

//USD/Hydra headers
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/containerDataSourceEditor.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

//This is the function where we filter prims
HdSceneIndexPrim ParentDataModifierSceneIndex::GetPrim(const SdfPath& primPath) const
{
    if (!_GetInputSceneIndex()){
        return HdSceneIndexPrim();
    }

    HdSceneIndexPrim prim   = _GetInputSceneIndex()->GetPrim(primPath);

    if (prim.dataSource && (primPath == _parentPath)){
        //Use an HdContainerDataSourceEditor to overwrite the values for the transform and visibility attributes
         prim.dataSource = 
                        HdContainerDataSourceEditor(prim.dataSource)
                        .Set(HdXformSchema::GetDefaultLocator(), HdXformSchema::Builder().SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(_transformMatrix)).Build())
                        .Set(HdVisibilitySchema::GetDefaultLocator(), HdVisibilitySchema::BuildRetained(HdRetainedTypedSampledDataSource<bool>::New(_visible)))
                        .Finish();
    }
        
    return prim;//returned the modified prim
}

}//End of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE