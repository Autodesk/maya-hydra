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

#include "flowViewport/sceneIndex/fvpDisplayStyleOverrideSceneIndex.h"

#include "pxr/imaging/hd/tokens.h"
#include "pxr/imaging/hd/legacyDisplayStyleSchema.h"
#include "pxr/imaging/hd/overlayContainerDataSource.h"
#include "pxr/imaging/hd/sceneIndexPrimView.h"
#include "pxr/imaging/hd/retainedDataSource.h"

namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

namespace DisplayStyleSceneIndex_Impl
{

using OptionalInt = DisplayStyleOverrideSceneIndex::OptionalInt;

struct _StyleInfo
{
    OptionalInt refineLevel;
    /// Retained data source storing refineLevel (or null ptr if empty optional
    /// value) to avoid allocating a data source for every prim.
    HdDataSourceBaseHandle refineLevelDs;
};

/// Data source for locator displayStyle.
class _DisplayStyleDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_DisplayStyleDataSource);

    HdDataSourceBaseHandle Get(const TfToken &name) override
    {
        if (name == HdLegacyDisplayStyleSchemaTokens->refineLevel) {
            return _styleInfo->refineLevelDs;
        }
        return nullptr;
    }

    TfTokenVector GetNames() override
    {
        static const TfTokenVector names = {
            HdLegacyDisplayStyleSchemaTokens->refineLevel
        };

        return names;
    }

private:
    _DisplayStyleDataSource(_StyleInfoSharedPtr const &styleInfo)
      : _styleInfo(styleInfo)
    {
    }

    _StyleInfoSharedPtr _styleInfo;
};

} // namespace DisplayStyleSceneIndex_Impl

using namespace DisplayStyleSceneIndex_Impl;

DisplayStyleOverrideSceneIndexRefPtr
DisplayStyleOverrideSceneIndex::New(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
{
    return TfCreateRefPtr(
        new DisplayStyleOverrideSceneIndex(
            inputSceneIndex));
}

DisplayStyleOverrideSceneIndex::
DisplayStyleOverrideSceneIndex(
    const HdSceneIndexBaseRefPtr &inputSceneIndex)
  : HdSingleInputFilteringSceneIndexBase(inputSceneIndex)
  , InputSceneIndexUtils(inputSceneIndex)
  , _styleInfo(std::make_shared<_StyleInfo>())
  , _overlayDs(
      HdRetainedContainerDataSource::New(
          HdLegacyDisplayStyleSchemaTokens->displayStyle,
          _DisplayStyleDataSource::New(_styleInfo)))
{
}

HdSceneIndexPrim
DisplayStyleOverrideSceneIndex::GetPrim(
    const SdfPath &primPath) const
{
    HdSceneIndexPrim prim = GetInputSceneIndex()->GetPrim(primPath);
    if (prim.dataSource) {
        if (!isExcluded(primPath) && prim.primType == HdPrimTypeTokens->mesh) {
            prim.dataSource =
                HdOverlayContainerDataSource::New(
                    _overlayDs, prim.dataSource);
        }
    }
    return prim;
}

SdfPathVector
DisplayStyleOverrideSceneIndex::GetChildPrimPaths(
    const SdfPath &primPath) const
{
    return GetInputSceneIndex()->GetChildPrimPaths(primPath);
}

void
DisplayStyleOverrideSceneIndex::SetRefineLevel(
    const OptionalInt &refineLevel)
{
    if (refineLevel == _styleInfo->refineLevel) {
        return;
    }

    _styleInfo->refineLevel = refineLevel;
    _styleInfo->refineLevelDs =
        refineLevel
            ? HdRetainedTypedSampledDataSource<int>::New(*refineLevel)
            : nullptr;

    static const HdDataSourceLocatorSet locators(
        HdLegacyDisplayStyleSchema::GetDefaultLocator()
            .Append(HdLegacyDisplayStyleSchemaTokens->refineLevel));

    _DirtyAllPrims(locators);
}

void
DisplayStyleOverrideSceneIndex::_DirtyAllPrims(
    const HdDataSourceLocatorSet &locators)
{
    if (!_IsObserved()) {
        return;
    }

    HdSceneIndexObserver::DirtiedPrimEntries entries;
    for (const SdfPath &path : HdSceneIndexPrimView(GetInputSceneIndex())) {
        entries.push_back({path, locators});
    }

    _SendPrimsDirtied(entries);
}

void
DisplayStyleOverrideSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    _SendPrimsAdded(entries);
}

void
DisplayStyleOverrideSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    _SendPrimsRemoved(entries);
}

void
DisplayStyleOverrideSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    if (!_IsObserved()) {
        return;
    }

    _SendPrimsDirtied(entries);
}

void DisplayStyleOverrideSceneIndex::addExcludedSceneRoot(
    const PXR_NS::SdfPath& sceneRoot
)
{
    _excludedSceneRoots.emplace(sceneRoot);
}

bool DisplayStyleOverrideSceneIndex::isExcluded(
    const PXR_NS::SdfPath& sceneRoot
) const
{
    for (const auto& excluded : _excludedSceneRoots) {
        if (sceneRoot.HasPrefix(excluded)) {
            return true;
        }
    }
    return false;
}

bool operator==(
    const DisplayStyleOverrideSceneIndex::OptionalInt &a,
    const DisplayStyleOverrideSceneIndex::OptionalInt &b)
{
    if (a.hasValue == false && b.hasValue == false) {
        return true;
    }

    return a.hasValue == b.hasValue && a.value == b.value;
}

bool operator!=(
    const DisplayStyleOverrideSceneIndex::OptionalInt &a,
    const DisplayStyleOverrideSceneIndex::OptionalInt &b)
{
    return !(a == b);
}

} //end of namespace FVP_NS_DEF
