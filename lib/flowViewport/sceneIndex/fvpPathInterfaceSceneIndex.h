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
#ifndef FVP_PATH_INTERFACE_SCENE_INDEX_H
#define FVP_PATH_INTERFACE_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

/// \class PathInterfaceSceneIndexBase
///
/// A simple pass-through filtering scene index that adds support for the path
/// interface.  Derived classes need only implement the 
/// PathInterface::SceneIndexPath() virtual.
///
class PathInterfaceSceneIndexBase 
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public PathInterface
    , public Fvp::InputSceneIndexUtils<PathInterfaceSceneIndexBase>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

protected:

    FVP_API
    PathInterfaceSceneIndexBase(
        const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex);

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;
};

}

#endif
