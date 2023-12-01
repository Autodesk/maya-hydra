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

//Local headers
#include "fvpFilteringSceneIndexExample.h"

//USD/Hydra headers
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/tokens.h>

PXR_NAMESPACE_OPEN_SCOPE

namespace 
{
    //As an example, we use a filtering scene index to filter mesh primitives which have more than 10 000 vertices.
    bool IsFiltered(const HdSceneIndexPrim& sceneIndexPrim)
    {
        static bool _hideCameras        = false;
        static bool _hideSimpleLights   = false;

        if (sceneIndexPrim.dataSource){
        
            if ((sceneIndexPrim.primType == HdPrimTypeTokens->mesh) || (sceneIndexPrim.primType == HdPrimTypeTokens->basisCurves) ){
                // Retrieve points from source mesh
                if (HdSampledDataSourceHandle pointsDs = HdPrimvarsSchema::GetFromParent(sceneIndexPrim.dataSource).GetPrimvar(HdPrimvarsSchemaTokens->points).GetPrimvarValue()) 
                {
                    VtValue v = pointsDs->GetValue(0.0f);
                    if (v.IsHolding<VtArray<GfVec3f>>()) {
                        const VtArray<GfVec3f>& points = v.Get<VtArray<GfVec3f>>();
                        const size_t numPoints = points.size();
                        if (numPoints > 10000){
                            //Hide the prims that have more than 10 000 vertices
                            return true;
                        }
                    }
                }
            }
        
            else 
            if (sceneIndexPrim.primType == HdPrimTypeTokens->camera){
                return _hideCameras;
            }else
            if (sceneIndexPrim.primType == HdPrimTypeTokens->simpleLight){
                return _hideSimpleLights;
            }
        }

        return false;
    }
}

namespace FVP_NS_DEF {

//This is the function where we filter prims
HdSceneIndexPrim FilteringSceneIndexExample::GetPrim(const SdfPath& primPath) const
{
    if (_GetInputSceneIndex()){
        const HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
        
        const bool isThisPrimFiltered = IsFiltered(prim);
        if ( ! isThisPrimFiltered){
            return prim;//return only non filtered prims
        }
    }

    return HdSceneIndexPrim();
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE