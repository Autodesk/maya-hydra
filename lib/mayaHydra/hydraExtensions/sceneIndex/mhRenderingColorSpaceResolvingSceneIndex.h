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

#ifndef MAYA_HYDRA_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H
#define MAYA_HYDRA_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H

// MayaHydra headers
#include "mayaHydraLib/api.h"

// Flow viewport toolkit headers
#include <flowViewport/sceneIndex/fvpRenderingColorSpaceResolvingSceneIndex.h>

namespace MAYAHYDRA_NS_DEF {

class MhRenderingColorSpaceResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<MhRenderingColorSpaceResolvingSceneIndex>
    MhRenderingColorSpaceResolvingSceneIndexRefPtr;

/// \class MhRenderingColorSpaceResolvingSceneIndex
///
/// A filtering scene index that resolves the rendering color space in the active
/// render settings prim. Boilerplate methods such as _PrimsAdded(), _PrimsRemoved(),
/// _PrimsDirtied() are implemented in the base class.
///
class MhRenderingColorSpaceResolvingSceneIndex : public Fvp::RenderingColorSpaceResolvingSceneIndex
{
public:
    MAYAHYDRALIB_API
    static MhRenderingColorSpaceResolvingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    MAYAHYDRALIB_API
    bool UseAuthoredRenderingColorSpace(
        const PXR_NS::TfToken& authoredRenderingColorSpace,
        const std::string&     applicationRenderingColorSpace) const override;

    MAYAHYDRALIB_API
    std::string GetApplicationRenderingColorSpace() const override;

    MAYAHYDRALIB_API
    bool IsKnownColorSpace(const std::string& colorSpace) const override;

    ~MhRenderingColorSpaceResolvingSceneIndex() override = default;

private:
    MhRenderingColorSpaceResolvingSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_HYDRA_RENDERING_COLOR_SPACE_RESOLVING_SCENE_INDEX_H
