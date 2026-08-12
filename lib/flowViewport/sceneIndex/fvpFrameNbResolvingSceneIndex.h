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
#ifndef FVP_FRAME_NB_RESOLVING_SCENE_INDEX_H
#define FVP_FRAME_NB_RESOLVING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

class FrameNbResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<FrameNbResolvingSceneIndex>
    FrameNbResolvingSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const FrameNbResolvingSceneIndex>
    FrameNbResolvingSceneIndexConstRefPtr;

/// \class FrameNbResolvingSceneIndex
///
/// A filtering scene index that resolves frame number hash-mark patterns in
/// render product names.  For renderSettings prims, it inspects each render
/// product's name data source.  Contiguous runs of '#' characters are
/// replaced with the current frame number (from HdSceneGlobalsSchema),
/// zero-padded or truncated to match the width of the '#' run.
///
/// For example, given frame 57 and product name "####frame#.jpg", the
/// resolved name is "0057frame7.jpg".
///
class FrameNbResolvingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public InputSceneIndexUtils<FrameNbResolvingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static FrameNbResolvingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~FrameNbResolvingSceneIndex() override = default;

protected:
    FrameNbResolvingSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
};

} // namespace FVP_NS_DEF

#endif // FVP_FRAME_NB_RESOLVING_SCENE_INDEX_H
