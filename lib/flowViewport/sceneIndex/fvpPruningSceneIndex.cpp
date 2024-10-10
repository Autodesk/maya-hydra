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

#include "flowViewport/sceneIndex/fvpPruningSceneIndex.h"
#include "fvpPruningSceneIndex.h"

#include <pxr/base/tf/staticTokens.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <iostream>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_PUBLIC_TOKENS(FvpPruningTokens, FVP_PRUNING_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

bool _PrunePrim(const HdSceneIndexPrim& prim, const TfToken& pruningToken)
{
    return true;
}

} // namespace

namespace FVP_NS_DEF {

PruningSceneIndexRefPtr PruningSceneIndex::New(const HdSceneIndexBaseRefPtr &inputSceneIndex)
{
    return TfCreateRefPtr(new PruningSceneIndex(inputSceneIndex));
}

PruningSceneIndex::PruningSceneIndex(HdSceneIndexBaseRefPtr const &inputSceneIndex) : 
    HdSingleInputFilteringSceneIndexBase(inputSceneIndex), 
    InputSceneIndexUtils(inputSceneIndex)
{
}

HdSceneIndexPrim PruningSceneIndex::GetPrim(const SdfPath& primPath) const
{
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector PruningSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

bool PruningSceneIndex::EnableFilter(const TfToken& pruningToken)
{
    if (_prunedPathsByFilter.find(pruningToken) != _prunedPathsByFilter.end()) {
        // Filter already enabled, no change needed.
        return false;
    }

    HdSceneIndexObserver::RemovedPrimEntries prunedPrims;

    for (const SdfPath& primPath: HdSceneIndexPrimView(GetInputSceneIndex())) {
        if (_PrunePrim(GetInputSceneIndex()->GetPrim(primPath), pruningToken)) {
            _prunedPathsByFilter[pruningToken].emplace(primPath);
            prunedPrims.emplace_back(primPath);
        }
    }

    if (!prunedPrims.empty()) {
        _SendPrimsRemoved(prunedPrims);
    }

    return true;
}

bool PruningSceneIndex::DisableFilter(const TfToken& pruningToken)
{
    if (_prunedPathsByFilter.find(pruningToken) == _prunedPathsByFilter.end()) {
        // Filter already disabled, no change needed.
        return false;
    }

    HdSceneIndexObserver::AddedPrimEntries unprunedPrims;

    for (const auto& primPath : _prunedPathsByFilter[pruningToken]) {
        unprunedPrims.emplace_back(primPath, GetInputSceneIndex()->GetPrim(primPath).primType);
    }

    if (!unprunedPrims.empty()) {
        _SendPrimsAdded(unprunedPrims);
    }

    return true;
}

void PruningSceneIndex::_PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries)
{
// Catch prims to prune
}

void PruningSceneIndex::_PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries)
{
// Remove from map if it was pruned
}

void PruningSceneIndex::_PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
// Check if prim pruning status should change
}

} // namespace FVP_NS_DEF
