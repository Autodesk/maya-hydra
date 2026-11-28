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
// sources composite same-API, Vulkan sources go through HgiInteropVulkan,
// which owns the cross-API semaphore and GL_EXT_memory_object protocol itself,
// and Metal sources through HgiInteropMetal's shared IOSurfaces. Vulkan and
// Metal frames are prepared on the render thread and only composited here.
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
        if (!_ShouldComposite(bool(srcDepth), dstRegion))
        {
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

#if defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
    HgiInteropFrame Prepare(
        Hgi*                   srcHgi,
        HgiTextureHandle const& srcColor,
        HgiTextureHandle const& srcDepth) override
    {
        // Only Vulkan and Metal: the copy into the interop's GL-readable
        // storage is the render thread's copy of the AOV, and moving it here
        // keeps the submission off Maya's thread. For Metal that also keeps
        // HgiMetal, which is not thread-safe, on the render thread alone. A
        // GL source has nothing to prepare, and keeps the render thread's own
        // copy pairs.
        if (!srcHgi || !srcColor
            || (srcHgi->GetAPIName() != HgiTokens->Vulkan
                && !(srcHgi->GetAPIName() == HgiTokens->Metal && CanPrepareMetalFrames())))
        {
            return {};
        }
        return _interop.PrepareTransfer(srcHgi, srcColor, srcDepth, HgiTokens->OpenGL);
    }

    void CompositePrepared(
        HgiInteropFrame const& frame,
        bool                   hasDepth,
        VtValue const&         dstFramebuffer,
        GfVec4i const&         dstRegion) override
    {
        if (!frame)
        {
            TF_CODING_ERROR("GlPresentBridge::CompositePrepared: no prepared frame");
            return;
        }
        if (!_ShouldComposite(hasDepth, dstRegion))
        {
            return;
        }
        _interop.TransferToApp(frame, dstFramebuffer, dstRegion);
    }
#endif

private:
    static bool _ShouldComposite(bool hasDepth, GfVec4i const& dstRegion)
    {
        // GetRenderTexture returns an empty handle when the pass has no depth
        // AOV and TransferToApp accepts that quietly, so manipulators and
        // wireframe silently sort against Maya's stale depth. Count it.
        if (!hasDepth)
        {
            RenderThreadStats::instance().RecordPresentWithoutDepth();
        }

        // A zero-area destination blits nothing and leaves the previous frame
        // in place, so the viewport appears frozen rather than reporting.
        if (dstRegion[2] <= 0 || dstRegion[3] <= 0)
        {
            RenderThreadStats::instance().RecordPresentSkipped();
            return false;
        }
        return true;
    }

    // Prepare runs on the render thread and the composites on the main thread.
    // HgiInterop supports that split for prepared frames only.
    HgiInterop _interop;
};

std::unique_ptr<RenderThreadPresentBridge> CreateGlPresentBridge()
{
    return std::make_unique<GlPresentBridge>();
}

bool CanPrepareMetalFrames()
{
#if defined(HGIINTEROP_HAS_METAL_PREPARE_TRANSFER)
    return true;
#else
    return false;
#endif
}

} // namespace MAYAHYDRA_NS_DEF
