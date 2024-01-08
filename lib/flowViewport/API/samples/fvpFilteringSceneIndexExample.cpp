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
#include <pxr/imaging/hd/xformSchema.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

namespace 
{
    //As an example, we use a filtering scene index to filter mesh primitives which have more than 10 000 vertices.
    bool ShouldBeFiltered(const HdSceneIndexPrim& sceneIndexPrim)
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
bool FilteringSceneIndexExample::IsFiltered(const SdfPath& primPath) const
{
    return _filteredPrims.find(primPath) != _filteredPrims.end();
}
void FilteringSceneIndexExample::UpdateFilteringStatus(const SdfPath& primPath)
{
    if (_GetInputSceneIndex() && ShouldBeFiltered(_GetInputSceneIndex()->GetPrim(primPath))) {
        _filteredPrims.insert(primPath);
    } else {
        _filteredPrims.erase(primPath);
    }
}

//This is the function where we filter prims
HdSceneIndexPrim FilteringSceneIndexExample::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim result;

    std::cout << "DBP : GetPrim " << primPath.GetAsString() << std::endl;
    if (_GetInputSceneIndex()) {
        HdSceneIndexPrim prim = _GetInputSceneIndex()->GetPrim(primPath);
        if (!ShouldBeFiltered(prim)) {
            result = prim; // return only non filtered prims
        }
    }

    return result;
}

void FilteringSceneIndexExample::_PrimsAdded(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    HdSceneIndexObserver::AddedPrimEntries filteredEntries;
    for (const auto& entry : entries) {
        std::cout << "DBP : _PrimsAdded " << entry.primPath.GetAsString() << std::endl;
        UpdateFilteringStatus(entry.primPath);
        if (!IsFiltered(entry.primPath)) {
            filteredEntries.push_back(entry);
        }
    }
    _SendPrimsAdded(filteredEntries);
}

void FilteringSceneIndexExample::_PrimsRemoved(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        std::cout << "DBP : _PrimsRemoved " << entry.primPath.GetAsString() << std::endl;
        _filteredPrims.erase(entry.primPath);
    }
    _SendPrimsRemoved(entries);
}

void FilteringSceneIndexExample::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    HdSceneIndexObserver::AddedPrimEntries   unfilteredPrims;
    HdSceneIndexObserver::RemovedPrimEntries filteredPrims;
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        std::cout << "DBP : _PrimsDirtied " << entry.primPath.GetAsString() << std::endl;
        bool wasFiltered = IsFiltered(entry.primPath);
        UpdateFilteringStatus(entry.primPath);
        if (wasFiltered == IsFiltered(entry.primPath)) {
            // Filtering status did not change, forward notification as-is
            dirtiedEntries.push_back(entry);
        }
        else {
            // Filtering status changed, dirty everything
            std::cout << "DBP : Filtering status changed from " << wasFiltered << " to "
                      << IsFiltered(entry.primPath) << " for " << entry.primPath.GetAsString() << std::endl;
            //HdDataSourceLocatorSet newSet = entry.dirtyLocators;
            //newSet.insert(HdXformSchema::GetDefaultLocator());
            //filteredEntries.push_back({ entry.primPath, HdDataSourceLocatorSet::UniversalSet() });
            if (wasFiltered) {
                    unfilteredPrims.emplace_back(
                        entry.primPath, _GetInputSceneIndex()->GetPrim(entry.primPath).primType);
            } else {
                    filteredPrims.emplace_back(entry.primPath);
            }
        }
    }
    _SendPrimsAdded(unfilteredPrims);
    _SendPrimsRemoved(filteredPrims);
    _SendPrimsDirtied(dirtiedEntries);
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE