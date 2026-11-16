// Copyright 2026 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "renderThreadPresentBridge.h"

#include "renderThreadStats.h"

// clang-format off
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(_MSC_VER)
#pragma warning(push)
#endif
// clang-format on

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgiInterop/hgiInterop.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF
{

// Composites the latest completed AOV into Maya's GL framebuffer via
// HgiInterop::TransferToApp, which dispatches on the source Hgi's API: GL
// sources composite same-API, and Vulkan sources go through HgiInteropVulkan,
// which owns the cross-API semaphore and GL_EXT_memory_object protocol itself.
class GlPresentBridge : public RenderThreadPresentBridge
{
public:
    GlPresentBridge() = default;
    ~GlPresentBridge() override = default;

    void Composite(
        Hgi*                   srcHgi,
        HgiTextureHandle const& srcColor,
        HgiTextureHandle const& srcDepth,
        VtValue const&         dstFramebuffer,
        GfVec4i const&         dstRegion) override
    {
        if (!srcHgi || !srcColor)
        {
            TF_CODING_ERROR("GlPresentBridge::Composite: null source Hgi or color texture");
            return;
        }

        // GetRenderTexture returns an empty handle when the pass has no depth
        // AOV and TransferToApp accepts that quietly, so manipulators and
        // wireframe silently sort against Maya's stale depth. Count it.
        if (!srcDepth)
        {
            RenderThreadStats::instance().RecordPresentWithoutDepth();
        }

        // A zero-area destination blits nothing and leaves the previous frame
        // in place, so the viewport appears frozen rather than reporting.
        if (dstRegion[2] <= 0 || dstRegion[3] <= 0)
        {
            RenderThreadStats::instance().RecordPresentSkipped();
            return;
        }

        // Manipulators and HUD are drawn current after the blit, so only scene
        // colour lags.
        _interop.TransferToApp(
            srcHgi,
            srcColor,
            srcDepth,
            HgiTokens->OpenGL, // dstApi: Maya is GL
            dstFramebuffer,
            dstRegion);
    }

    TfToken GetSourceApiName(Hgi* srcHgi) const override
    {
        if (!srcHgi) return TfToken();
        return srcHgi->GetAPIName();
    }

private:
    HgiInterop _interop;
};

std::unique_ptr<RenderThreadPresentBridge> CreateGlPresentBridge()
{
    return std::make_unique<GlPresentBridge>();
}

} // namespace MAYAHYDRA_NS_DEF
