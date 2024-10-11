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
#ifndef FVP_PRUNING_SCENE_INDEX_H
#define FVP_PRUNING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"

#include <pxr/base/tf/diagnosticLite.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hdsi/api.h>
#include <pxr/imaging/hd/materialFilteringSceneIndexBase.h>
#include <pxr/imaging/hd/materialNetworkInterface.h>

#include <set>
#include <unordered_map>

PXR_NAMESPACE_OPEN_SCOPE

// clang-format off
#define FVP_PRUNING_TOKENS \
    (mesh) \
    (sphere) \
    (cone) \
    (cube)
// clang-format on

TF_DECLARE_PUBLIC_TOKENS(FvpPruningTokens, FVP_API, FVP_PRUNING_TOKENS);

PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

class PruningSceneIndex;
typedef PXR_NS::TfRefPtr<PruningSceneIndex> PruningSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const PruningSceneIndex> PruningSceneIndexConstRefPtr;

class PruningSceneIndex :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<PruningSceneIndex>
    , public PathInterface // As a workaround until we move to exclusively using PathMappers
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static PruningSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr &inputScene);

    FVP_API
    ~PruningSceneIndex() override = default;

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    bool EnableFilter(const PXR_NS::TfToken& filterToken);

    FVP_API
    bool DisableFilter(const PXR_NS::TfToken& filterToken);

    FVP_API
    void AddExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot);

    FVP_API
    PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override {
        PXR_NAMESPACE_USING_DIRECTIVE;
        const PathInterface* pathInterface = dynamic_cast<const PathInterface*>(&*GetInputSceneIndex());
        TF_AXIOM(pathInterface);
        return pathInterface->UfePathToPrimSelections(appPath);
    }

protected:
    FVP_API
    PruningSceneIndex(PXR_NS::HdSceneIndexBaseRefPtr const &inputSceneIndex);

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

    FVP_API
    bool _IsExcluded(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    bool _PrunePrim(const PXR_NS::SdfPath& primPath, const PXR_NS::HdSceneIndexPrim& prim, const PXR_NS::TfToken& pruningToken) const;

    FVP_API
    bool _IsAncestorPrunedInclusive(const PXR_NS::SdfPath& primPath) const;

    // Maps a filtering token to the set of prim paths that have been pruned out by this token
    std::map<PXR_NS::TfToken, PXR_NS::SdfPathSet> _prunedPathsByFilter;
    std::map<PXR_NS::SdfPath, std::set<PXR_NS::TfToken>> _filtersByPrunedPath;

    std::set<PXR_NS::SdfPath> _excludedSceneRoots;
};

} // namespace FVP_NS_DEF

#endif // FVP_PRUNING_SCENE_INDEX_H
