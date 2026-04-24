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

// MayaHydra headers
#include "mayaHydraLib/api.h"

// Usd/Hydra headers 
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hdGp/generativeProceduralResolvingSceneIndex.h>

// Flow viewport toolkit headers
#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>

#include <set>

namespace MAYAHYDRA_NS_DEF {

class MhGenerativeProceduralResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<MhGenerativeProceduralResolvingSceneIndex>
    MhGenerativeProceduralResolvingSceneIndexRefPtr;

/// \class MhGenerativeProceduralResolvingSceneIndex
/// Wraps HdGp's generative-procedural resolving scene index for the MayaHydra viewport chain.
///
/// Render setup places scene globals before this index (procedurals need the current frame)
/// and selection after (generated prims must exist for highlighting). The `Mh*` wrapper keeps
/// HdGp construction in mayaHydraLib and aligns with other MayaHydra indices (e.g. excluded roots),
/// instead of inlining `HdGpGenerativeProceduralResolvingSceneIndex::New` in the plugin.
class MhGenerativeProceduralResolvingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MhGenerativeProceduralResolvingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    MAYAHYDRALIB_API
    static MhGenerativeProceduralResolvingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    MAYAHYDRALIB_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~MhGenerativeProceduralResolvingSceneIndex() override = default;

protected:
    MhGenerativeProceduralResolvingSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex)
        : PXR_NS::HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
        , InputSceneIndexUtils(inputSceneIndex)
    {
    }

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

    void _DirtyDescendantsXform(
        const PXR_NS::SdfPath&                            path,
        const PXR_NS::HdDataSourceLocatorSet&             locators,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries);

private:
    std::set<PXR_NS::SdfPath> _generativeProceduralPaths;

};

} // namespace MAYAHYDRA_NS_DEF
#endif