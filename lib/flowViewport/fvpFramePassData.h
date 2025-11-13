//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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
#ifndef FLOW_VIEWPORT_FRAME_PASS_FILTERING_DATA_H
#define FLOW_VIEWPORT_FRAME_PASS_FILTERING_DATA_H

#include "flowViewport/api.h"

#ifdef VIEWPORT_TOOLBOX

#include <pxr/pxr.h>
#include <pxr/base/tf/token.h>
#include <pxr/usd/sdf/path.h>

#include "fvpUtils.h"

#include <hvt/engine/renderIndexProxy.h>
#include <hvt/engine/framePass.h>

#include <functional>
#include <memory>
#include <set>
#include <vector>

namespace FVP_NS_DEF {

/*! \brief Data structure containing all per-pass information
 *
 * This struct contains all the configuration data needed for a frame pass such as filtering
 * primitives in a frame pass, including renderer selection, paths,
 * render tags, and light handling preferences.
 */
struct FramePassData
{
    //! Name of the renderer to use for this pass
    PXR_NS::TfToken _rendererName;

    //! Function to update render tags
    std::function<
        void(bool includeRenderPurpose, bool includeProxyPurpose, bool includeGuidePurpose)>
        _renderTagsUpdateFn;

    //! Current render tags to include for this pass (updated dynamically)
    std::set<PXR_NS::TfToken> _includeRenderTags;

    //! Current HVT frame pass
    hvt::FramePassPtr   _framePass;

    //!< Render index proxy for this pass
    hvt::RenderIndexProxyPtr _renderIndexProxy; 
    
    //! Paths to force include in this frame pass
    PXR_NS::SdfPathVector _includePaths;

    //! Paths to force exclude from this frame pass
    PXR_NS::SdfPathVector _excludePaths;

    //! this frame pass supports prims with no purpose render tags, set this to true if you want to
    //! render prims that do not have a purpose render tag
    bool _supportPrimsWithNoPurposeRenderTag = false;

    //! The filtering scene index for this pass, we cannot use Fvp::PassFilteringSceneIndex as it
    //! also includes this class declaration
    PXR_NS::HdSingleInputFilteringSceneIndexBaseRefPtr _passFilteringSceneIndex;

    //! Helper methods to safely access the frame pass
    bool IsValid() const { return _framePass != nullptr; }
    bool HasRenderIndexProxy() const { return _renderIndexProxy != nullptr; }
    const hvt::FramePassPtr& GetFramePass() const { return _framePass; }
    hvt::RenderIndexProxyPtr GetRenderIndexProxy() const { return _renderIndexProxy; }
    void
    SetPassFilteringSceneIndex(const PXR_NS::HdSingleInputFilteringSceneIndexBaseRefPtr& sceneIndex)
    {
        _passFilteringSceneIndex = sceneIndex;
    }

    FVP_API
    void DirtyPrimsFromPurposeRenderTag(const PXR_NS::TfToken purposeRenderTag);
};

//! Shared pointer type for FramePassData to enable sharing between scene indices
using FramePassDataPtr = std::shared_ptr<FramePassData>;
using FramePassConstDataPtr = std::shared_ptr<const FramePassData>;

//! Container type for multiple frame pass filtering data
using FramePassDataPtrVector = std::vector<FramePassDataPtr>;

}; // namespace FVP_NS_DEF

#endif // VIEWPORT_TOOLBOX

#endif // FLOW_VIEWPORT_FRAME_PASS_FILTERING_DATA_H