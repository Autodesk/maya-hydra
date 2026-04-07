//
// Copyright 2026 Autodesk
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

#ifndef MAYA_HYDRA_GENERATIVE_PROCEDURAL_RESOLVING_SCENE_INDEX_H
#define MAYA_HYDRA_GENERATIVE_PROCEDURAL_RESOLVING_SCENE_INDEX_H

#include "mayaHydraLib/api.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hdGp/generativeProceduralResolvingSceneIndex.h>

#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>

namespace MAYAHYDRA_NS_DEF {

class MhGenerativeProceduralResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<MhGenerativeProceduralResolvingSceneIndex>
    MhGenerativeProceduralResolvingSceneIndexRefPtr;

class MhGenerativeProceduralResolvingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MhGenerativeProceduralResolvingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    static MhGenerativeProceduralResolvingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex)
    {
        return PXR_NS::TfCreateRefPtr(new MhGenerativeProceduralResolvingSceneIndex(
            PXR_NS::HdGpGenerativeProceduralResolvingSceneIndex::New(inputSceneIndex)));
    }

    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetPrim(primPath);
    }

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~MhGenerativeProceduralResolvingSceneIndex() override = default;

    MAYAHYDRALIB_API
    void AddExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot);

protected:
    MhGenerativeProceduralResolvingSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex)
        : PXR_NS::HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
        , InputSceneIndexUtils(inputSceneIndex)
    {
    }

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override
    {
        if (!_IsObserved())
            return;
        _SendPrimsAdded(entries);
    }

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override
    {
        if (!_IsObserved())
            return;
        _SendPrimsRemoved(entries);
    }

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase& sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override
    {
        if (!_IsObserved())
            return;
        _SendPrimsDirtied(entries);
    }

private:
    std::set<PXR_NS::SdfPath> _excludedSceneRoots;
};

} // namespace MAYAHYDRA_NS_DEF
#endif