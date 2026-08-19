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

#ifndef FVP_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H
#define FVP_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <string>

namespace FVP_NS_DEF {
class RenderingColorSpaceResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<RenderingColorSpaceResolvingSceneIndex>
    RenderingColorSpaceResolvingSceneIndexRefPtr;

/// \class RenderingColorSpaceResolvingSceneIndex
///
/// A base class for filtering scene index that resolves the rendering color space in the active
/// render settings prim. It is used to create a derived class for each DCC.
///
/// Derived class's GetPrim() should only set the rendering color space on the active render
/// settings prim.
///
class RenderingColorSpaceResolvingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<RenderingColorSpaceResolvingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    virtual std::string GetRenderingColorSpaceFromDCC() const = 0;
    FVP_API
    virtual bool        IsKnownColorSpace(const std::string& colorSpace) const = 0;

    ~RenderingColorSpaceResolvingSceneIndex() override = default;

protected:
    FVP_API
    RenderingColorSpaceResolvingSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
};
} // end of namespace FVP_NS_DEF

#endif // FVP_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H
