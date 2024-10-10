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

#include <pxr/base/tf/staticTokens.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexPrimView.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

#include <iostream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

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

void PruningSceneIndex::_PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries)
{

}

void PruningSceneIndex::_PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries)
{

}

void PruningSceneIndex::_PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries)
{

}

} // namespace FVP_NS_DEF
