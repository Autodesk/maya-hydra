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

#include "flowViewport/sceneIndex/fvpPathInterfaceSceneIndex.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

PathInterfaceSceneIndexBase::
PathInterfaceSceneIndexBase(HdSceneIndexBaseRefPtr const &inputSceneIndex)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
  , InputSceneIndexUtils(inputSceneIndex)
{}

HdSceneIndexPrim
PathInterfaceSceneIndexBase::GetPrim(const SdfPath &primPath) const
{
    return GetInputSceneIndex()->GetPrim(primPath);
}

SdfPathVector
PathInterfaceSceneIndexBase::GetChildPrimPaths(const SdfPath &primPath) const
{
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
PathInterfaceSceneIndexBase::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    _SendPrimsAdded(entries);
}

void
PathInterfaceSceneIndexBase::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    _SendPrimsDirtied(entries);
}

void
PathInterfaceSceneIndexBase::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    _SendPrimsRemoved(entries);
}

}
