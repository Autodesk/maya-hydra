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

#ifndef MAYAHYDRA_RENDER_THREAD_PRESENT_BRIDGE_H
#define MAYAHYDRA_RENDER_THREAD_PRESENT_BRIDGE_H

// Graphics-context and present bridge interface.
//
// The render thread owns the Hgi and the render index; the main thread owns
// Maya's GL framebuffer. The present bridge is the seam between them: the main
// thread composites the latest completed AOV (color + depth) into Maya's FBO at
// the start of the render op. Maya's viewport is always GL while the renderer
// may use any API, so all cross-API interop is confined here. The bridge does
// not own the AOV textures; it reads the handoff's completed-frame snapshot.

#include <mayaHydraLib/api.h>

// clang-format off
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"
#elif defined(_MSC_VER)
#pragma warning(push)
#endif
// clang-format on

#include <pxr/base/gf/vec4i.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/vt/value.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/texture.h>
#include <pxr/imaging/hgiInterop/hgiInterop.h>

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

#include <cstdint>

namespace MAYAHYDRA_NS_DEF {

/// \class RenderThreadPresentBridge
///
/// Abstract interface for the present composite. The main thread calls
/// Composite() at the start of the render op; the render thread produces the
/// AOVs and publishes them through the handoff protocol.
class MAYAHYDRALIB_API RenderThreadPresentBridge
{
public:
    virtual ~RenderThreadPresentBridge() = default;

    /// Composite the latest completed AOV into the destination GL framebuffer.
    /// Called on the main thread with the completed-frame handles published
    /// through the handoff protocol; the caller must wait on the GPU fence
    /// first.
    ///
    /// \p srcDepth May be empty. Blitted alongside colour so VP2 overlays
    ///    depth-compose correctly against the one-frame-stale scene.
    /// \p dstFramebuffer GL framebuffer object name in a VtValue. When empty,
    ///    the currently bound framebuffer is used.
    /// \p dstRegion (left, BOTTOM, width, height), as OpenGL viewport coords.
    virtual void Composite(
        pxr::Hgi*                   srcHgi,
        pxr::HgiTextureHandle const& srcColor,
        pxr::HgiTextureHandle const& srcDepth,
        pxr::VtValue const&         dstFramebuffer,
        pxr::GfVec4i const&         dstRegion) = 0;

#if defined(HGIINTEROP_HAS_PREPARE_TRANSFER)
    /// Called on the render thread after a render: copy the AOV into the
    /// bridge's own storage, without GL, for CompositePrepared. Returns an
    /// empty frame when the source needs no preparation, in which case the
    /// caller keeps its own copy of the AOV and calls Composite instead.
    virtual pxr::HgiInteropFrame Prepare(
        pxr::Hgi*                   srcHgi,
        pxr::HgiTextureHandle const& srcColor,
        pxr::HgiTextureHandle const& srcDepth) = 0;

    /// Composite a frame returned by Prepare. Called on the main thread; the
    /// interop orders the composite against the copy on the GPU, so there is
    /// no fence to wait on first. \p hasDepth is for diagnostics only.
    virtual void CompositePrepared(
        pxr::HgiInteropFrame const& frame,
        bool                        hasDepth,
        pxr::VtValue const&         dstFramebuffer,
        pxr::GfVec4i const&         dstRegion) = 0;
#endif

    /// Returns the API name of the source Hgi (e.g. "OpenGL", "Vulkan").
    virtual pxr::TfToken GetSourceApiName(pxr::Hgi* srcHgi) const = 0;
};

/// Factory: create the GL present bridge, which composites via
/// HgiInterop::TransferToApp into Maya's FBO.
MAYAHYDRALIB_API std::unique_ptr<RenderThreadPresentBridge> CreateGlPresentBridge();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_PRESENT_BRIDGE_H
