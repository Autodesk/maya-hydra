<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Metal on the Render Thread

On macOS, Hydra renders with HgiMetal while Maya's viewport must be in OpenGL
mode: the render override only supports OpenGL draw APIs, and frames reach
Maya's framebuffer through `HgiInteropMetal`, which shares IOSurfaces between
the two APIs.  Render-thread pipelining works there by following the
[Vulkan design](vulkanRenderThread.md) closely; this describes where it
differs.

As with Vulkan, everything keys off the Hgi's API name.

## No context to make current

Metal submits to a command queue rather than to a current context, so the
render thread receives a null context and skips both `MakeCurrent` and the
`HgiGL` context arena.  The per-frame fence is a no-op, as under Vulkan.

## HgiMetal stays on one thread at a time

`HgiVulkan` was made safe to submit from several threads.  `HgiMetal` was
not: it keeps one primary command buffer and the frame and encoder state
around it, with no locks.  The Metal route instead relies on HgiMetal being
used by one thread at a time:

- While the render thread runs, it is the only thread that uses HgiMetal.
  The main thread only issues GL to composite.
- Everything else that renders on the main thread (the first frame, the
  first after a resize, picks) waits for the render thread to go idle first,
  as it must anyway for the scene index.  A time change while stopped is
  rendered on the render thread, like any other frame once it is primed; the
  render op only waits for it before compositing it.

Code that uses the Hgi from the main thread while the render thread is busy
breaks this.  GPU buffer sharing and outline selection highlighting would, but
both are off on macOS.

## Autorelease pools

Metal hands out autoreleased Objective-C objects.  The main thread's event
loop drains them there; nothing drains them on the render thread.  HgiMetal
only drains its own pool between `StartFrame` and `EndFrame`, so the render
thread wraps each request, and its teardown, in an autorelease pool.

## The AOV copy is the interop's

As under Vulkan, the render thread calls the present bridge's `Prepare`, and
`HgiInterop::PrepareTransfer` copies the frame into a slot of interop-owned
storage.  For Metal that is a compute pass, on HgiMetal's primary command
buffer so it runs after the frame's render work, into a pair of
IOSurface-backed textures, one for colour and one for encoded depth.  It makes
no GL calls.  The main thread then composites the slot with
`HgiInterop::TransferToApp(frame, ...)`, through GL textures bound to the same
IOSurfaces.  The render thread's copy pairs are not used.

`HgiInteropMetal` keeps a ring of up to three slots, growing from one only
when every slot is in use.  macOS OpenGL has no semaphores to share with
Metal, so the two directions are ordered by waits on the main thread rather
than on the GPU:

- Before drawing a slot, the main thread waits until the command buffer
  holding its copy has been scheduled, as the synchronous path always has.
- After drawing a slot, GL places a fence.  When a newer slot is composited,
  the main thread waits on the older slot's fence before handing it back to
  the render thread.  That draw was issued a frame earlier, so the wait
  normally returns at once.

Both waits are on the main thread.  The render thread never waits on GL, and
it blocks only when every slot is in use, which lasts until the main thread's
next composite.

## Build requirements

The Metal copy needs `HgiInterop::PrepareTransfer` to copy Metal textures.
`hgiInterop.h` defines `HGIINTEROP_HAS_METAL_PREPARE_TRANSFER` when it does.
Against a USD without it, the render thread does not start under Metal and
drawing stays synchronous, with a warning.  The alternative would composite
the live AOV and drive HgiMetal from both threads.

The render thread links `libobjc` on macOS for its autorelease pools.
