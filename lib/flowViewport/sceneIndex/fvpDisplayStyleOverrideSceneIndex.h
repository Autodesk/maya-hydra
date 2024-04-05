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
#ifndef FVP_DISPLAY_STYLE_OVERRIDE_SCENE_INDEX_H
#define FVP_DISPLAY_STYLE_OVERRIDE_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include "pxr/imaging/hdsi/api.h"
#include "pxr/imaging/hd/filteringSceneIndex.h"

#include <set>

namespace FVP_NS_DEF {

namespace DisplayStyleSceneIndex_Impl
{
struct _StyleInfo;
using _StyleInfoSharedPtr = std::shared_ptr<_StyleInfo>;
}

class DisplayStyleOverrideSceneIndex;
typedef PXR_NS::TfRefPtr<DisplayStyleOverrideSceneIndex> DisplayStyleOverrideSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const DisplayStyleOverrideSceneIndex> DisplayStyleOverrideSceneIndexConstRefPtr;

///
/// \class DisplayStyleOverrideSceneIndex
///
/// A scene index overriding the display style for each prim.
///
class DisplayStyleOverrideSceneIndex :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<DisplayStyleOverrideSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static DisplayStyleOverrideSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex);

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

    /// A replacement for std::optional<int> that is not available until C++17.
    struct OptionalInt
    {
        bool hasValue = false;
        int value = 0;

        operator bool() const { return hasValue; }
        int operator*() const { return value; }
    };

    /// Sets the refine level (at data source locator displayStyle:refineLevel)
    /// for every prim in the input scene inedx.
    ///
    /// If an empty optional value is provided, a null data source will be
    /// returned for the data source locator.
    ///
    FVP_API
    void SetRefineLevel(const OptionalInt &refineLevel);

    FVP_API
    void addExcludedSceneRoot(const PXR_NS::SdfPath& sceneRoot);

protected:
    FVP_API
    DisplayStyleOverrideSceneIndex(
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

private:
    void _DirtyAllPrims(const PXR_NS::HdDataSourceLocatorSet &locators);

    bool isExcluded(const PXR_NS::SdfPath& sceneRoot) const;

    std::set<PXR_NS::SdfPath> _excludedSceneRoots;

    DisplayStyleSceneIndex_Impl::
    _StyleInfoSharedPtr const _styleInfo;

    /// Prim overlay data source.
    PXR_NS::HdContainerDataSourceHandle const _overlayDs;
};

HDSI_API
bool operator==(
    const DisplayStyleOverrideSceneIndex::OptionalInt &a,
    const DisplayStyleOverrideSceneIndex::OptionalInt &b);

HDSI_API
bool operator!=(
    const DisplayStyleOverrideSceneIndex::OptionalInt &a,
    const DisplayStyleOverrideSceneIndex::OptionalInt &b);

} //end of namespace FVP_NS_DEF

#endif //FVP_DISPLAY_STYLE_OVERRIDE_SCENE_INDEX_H
