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

#include <stack>

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
    if (ShouldBeFiltered(GetInputSceneIndex()->GetPrim(primPath))) {
        _filteredPrims.insert(primPath);
    } else {
        _filteredPrims.erase(primPath);
    }
}

FilteringSceneIndexExample::FilteringSceneIndexExample(const HdSceneIndexBaseRefPtr& inputSceneIndex)
    : ParentClass(inputSceneIndex), InputSceneIndexUtils(inputSceneIndex)
{
    std::stack<SdfPath> primPathsToTraverse({ SdfPath::AbsoluteRootPath() });
    while (!primPathsToTraverse.empty()) {
        SdfPath currPrimPath = primPathsToTraverse.top();
        primPathsToTraverse.pop();
        UpdateFilteringStatus(currPrimPath);
        for (const auto& childPath : inputSceneIndex->GetChildPrimPaths(currPrimPath)) {
            primPathsToTraverse.push(childPath);
        }
    }
}

HdSceneIndexPrim FilteringSceneIndexExample::GetPrim(const SdfPath& primPath) const
{
    return IsFiltered(primPath) ? HdSceneIndexPrim() : GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector FilteringSceneIndexExample::GetChildPrimPaths(const SdfPath& primPath) const {
    // A filtered prim should not exist from the point of view of downstream scene indices,
    // so return an empty vector if the current prim is filtered. This case should normally
    // not be reached during scene index hierarchy traversal, as its parent should not even
    // return it when GetChildPrimPaths is called on it (see other comment in this method.)
    if (IsFiltered(primPath)) {
        return SdfPathVector();
    }

    // If the current prim is not filtered, we still do not want to return a path
    // to a filtered child prim, as a filtered prim should not exist at all (and
    // we might have sent a PrimsRemoved notification prior). Thus, remove all
    // child paths to filtered prims before returning.
    SdfPathVector childPaths = GetInputSceneIndex()->GetChildPrimPaths(primPath);
    childPaths.erase(
        std::remove_if(
            childPaths.begin(),
            childPaths.end(),
            [this](const SdfPath& childPath) -> bool { return IsFiltered(childPath); }),
        childPaths.end());
    return childPaths;
}

void FilteringSceneIndexExample::_PrimsAdded(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::AddedPrimEntries& entries)
{
    HdSceneIndexObserver::AddedPrimEntries unfilteredEntries;
    for (const auto& entry : entries) {
        // We only want to forward the notifications for prims that don't get filtered out
        UpdateFilteringStatus(entry.primPath);
        if (!IsFiltered(entry.primPath)) {
            unfilteredEntries.push_back(entry);
        }
    }
    _SendPrimsAdded(unfilteredEntries);
}

void FilteringSceneIndexExample::_PrimsRemoved(
    const HdSceneIndexBase&                       sender,
    const HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    for (const auto& entry : entries) {
        // We don't need to update or check the filtering status, since the prim is getting removed either way
        _filteredPrims.erase(entry.primPath);
    }
    _SendPrimsRemoved(entries);
}

void FilteringSceneIndexExample::_PrimsDirtied(
    const HdSceneIndexBase&                         sender,
    const HdSceneIndexObserver::DirtiedPrimEntries& entries)
{
    // There are three potential scenarios here for a given prim :
    // 1. Its filtering status did NOT change -> forward the PrimsDirtied notification as-is
    // 2. Its filtering status DID change :
    //    2a. If the prim was previously filtered -> it is now unfiltered, so send a PrimsAdded notification
    //    2b. If the prim was previously unfiltered -> it is now filtered, so send a PrimsRemoved notification
    HdSceneIndexObserver::AddedPrimEntries   newlyUnfilteredEntries;
    HdSceneIndexObserver::RemovedPrimEntries newlyFilteredEntries;
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedEntries;
    for (const auto& entry : entries) {
        bool wasPreviouslyFiltered = IsFiltered(entry.primPath);
        UpdateFilteringStatus(entry.primPath);
        if (wasPreviouslyFiltered == IsFiltered(entry.primPath)) {
            // Filtering status did not change, forward notification as-is
            dirtiedEntries.push_back(entry);
        }
        else {
            // Filtering status changed, send a different notification instead
            if (wasPreviouslyFiltered) {
                    newlyUnfilteredEntries.emplace_back(
                        entry.primPath, GetInputSceneIndex()->GetPrim(entry.primPath).primType);
            } else {
                    newlyFilteredEntries.emplace_back(entry.primPath);
            }
        }
    }
    _SendPrimsAdded(newlyUnfilteredEntries);
    _SendPrimsRemoved(newlyFilteredEntries);
    _SendPrimsDirtied(dirtiedEntries);
}

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE
