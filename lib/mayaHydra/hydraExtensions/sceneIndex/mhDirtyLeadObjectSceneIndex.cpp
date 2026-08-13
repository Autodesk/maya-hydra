//
// Copyright 2024 Autodesk
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
#include "mhDirtyLeadObjectSceneIndex.h"

#include <flowViewport/tokens.h>

// Hydra headers
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/usd/sdf/path.h>

#include <stack>

PXR_NAMESPACE_USING_DIRECTIVE

// The MhDirtyLeadObjectSceneIndex class is responsible for dirtying the current and previous maya selection lead objects prim
// path when a change in the lead object selection has happened.
namespace MAYAHYDRA_NS_DEF {

DEFINE_PRIVATE_OVERRIDEWIREFRAMECOLOR_TOKEN

static const HdDataSourceLocatorSet primvarsColorsLocatorSet{ primvarsOverrideWireframeColorLocator,
                                                            HdPrimvarsSchema::GetDefaultLocator().Append(HdTokens->displayColor)
                                                         };

void MhDirtyLeadObjectSceneIndex::dirtyLeadObjectRelatedSelections(const Fvp::PrimSelections& previousLeadObjectPrimSelections, const Fvp::PrimSelections& currentLeadObjectPrimSelections)
{
    // Each SdfPath could be a hierarchy path, so we need to get the children prim paths
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedPrimEntries;
    for (const auto& previousLeadObjectPrimSelection : previousLeadObjectPrimSelections) {
        _DirtyPrimSelectionRecursively(previousLeadObjectPrimSelection, dirtiedPrimEntries);
    }
    for (const auto& currentLeadObjectPrimSelection : currentLeadObjectPrimSelections) {
        _DirtyPrimSelectionRecursively(currentLeadObjectPrimSelection, dirtiedPrimEntries);
    }

    if (! dirtiedPrimEntries.empty()){
        _SendPrimsDirtied(dirtiedPrimEntries);
    }
}

void MhDirtyLeadObjectSceneIndex::dirtySelectionRelatedPrims(const SdfPathVector& primPaths)
{
    HdSceneIndexObserver::DirtiedPrimEntries dirtiedPrimEntries;
    for (const auto& primPath : primPaths) {
        // Reuses the lead-object traversal: a selected path can be a hierarchy root, and instancers
        // have to reach their prototypes, which is exactly the same walk.
        _DirtyPrimSelectionRecursively(Fvp::PrimSelection { primPath }, dirtiedPrimEntries);
    }

    if (! dirtiedPrimEntries.empty()){
        _SendPrimsDirtied(dirtiedPrimEntries);
    }
}

void MhDirtyLeadObjectSceneIndex::_DirtyPrimSelectionRecursively(const Fvp::PrimSelection& primSelection, HdSceneIndexObserver::DirtiedPrimEntries& inoutDirtiedPrimEntries)const
{
    //path can be a hierachy of prim paths so we need to get all children prim paths
    std::stack<SdfPath> pathsToDirty({primSelection.primPath});
    while (!pathsToDirty.empty()) {
        auto currPathToDirty = pathsToDirty.top();
        pathsToDirty.pop();

        inoutDirtiedPrimEntries.emplace_back(currPathToDirty, primvarsColorsLocatorSet);

        for (const auto& childPath : GetChildPrimPaths(currPathToDirty)) {
            pathsToDirty.push(childPath);
        }
        
        HdSceneIndexPrim currPrim = GetInputSceneIndex()->GetPrim(currPathToDirty);
        if (currPrim.primType == HdPrimTypeTokens->instancer) {
            HdInstancerTopologySchema instancerTopology = HdInstancerTopologySchema::GetFromParent(currPrim.dataSource);
            for (const auto& prototypePath : instancerTopology.GetPrototypes()->GetTypedValue(0)) {
                pathsToDirty.push(prototypePath);
            }
        }
    }
}

}//end of namespace MAYAHYDRA_NS_DEF
