<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Vulkan on the Render Thread

Render-thread pipelining works with either graphics backend, but three of its
mechanisms assume OpenGL.  This describes what changes when the Hgi is Vulkan.

Everything here keys off the Hgi's API name rather than off whichever setting
selected it, so a session that asked for Vulkan and fell back to the platform
default behaves exactly like an ordinary GL run.

## No context to make current

The render thread normally receives a GL context, shared with Maya's, that it
makes current for its whole lifetime.  Vulkan submits to device queues rather
than to a current context, so the thread receives a null context instead and
skips both `MakeCurrent` and the `HgiGL` context arena.

The `contextArenaAcquired` stat still reports true under Vulkan.  It means "the
thread has what it needs to render off-thread", which holds; it does not mean
an arena was allocated.

## No fence to signal

The per-frame fence that orders the main thread's composite behind the render
is a GL sync object, which is neither available nor needed on a Vulkan render
thread.  `RenderThreadFence::CreateAndSignal` returns a no-op fence there, and
the ordering falls to the semaphore pair inside `HgiInteropVulkan`.

## A backend-specific AOV copy

The render thread copies each finished frame's AOVs into storage it owns, so it
can start the next frame while the main thread composites the previous one.
Hgi has no backend-neutral texture-to-texture GPU copy, so this is the one
place that must branch on the backend:

- OpenGL copies directly with `glCopyImageSubData` on the render thread's
  context.
- Vulkan records a `HgiVulkanBlitCmds::BlitTexture` and submits it with
  `HgiSubmitWaitTypeNoWait`.

Both are issued before the frame's fence is created, so the fence covers the
copy as well as the render.  A backend that is neither warns and drops the
frame rather than publishing a handle to the live AOV, which would be read
while the next frame overwrites it.

## Build requirements

The Vulkan copy reaches `hgiVulkan` directly, as does the Vulkan route of
[GPU buffer sharing](gpuBufferSharing.md), and not every USD package exports
it.  `FindUSD.cmake` sets `USD_HAS_HGI_VULKAN` when the `hgiVulkan` target
exists; `mayaHydraLib` then links it and defines `USD_HAS_HGI_VULKAN`, and the
Vulkan copy compiles out otherwise.  A build without it still runs the GL path
unchanged.

## Secondary-thread submission in USD

Running Vulkan from a render thread also requires USD-side changes, because
`HgiVulkan` rejected submission from any thread but the one that created it.
Those changes replace the thread-identity checks in `HgiVulkan::_SubmitCmds`
and `_EndFrameSync` with a submission mutex in `HgiVulkanCommandQueue`, which
is what the Vulkan specification requires of a caller sharing a device queue.

A build whose USD lacks them reports a coding error from the render thread
rather than drawing.
