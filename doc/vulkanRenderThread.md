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

## No fence to signal

The per-frame fence that orders the main thread's composite behind the render
is a GL sync object, which is neither available nor needed on a Vulkan render
thread.  `RenderThreadFence::CreateAndSignal` returns a no-op fence there, and
the ordering falls to the semaphores inside `HgiInteropVulkan` described below.

## The AOV copy is the interop's

The render thread copies each finished frame's AOVs into storage it owns, so it
can start the next frame while the main thread composites the previous one.
Under OpenGL that copy is `glCopyImageSubData` into one of three render-thread
copy pairs, ordered against the composite with GL fences.

Under Vulkan the render thread instead calls the present bridge's `Prepare`,
which calls `HgiInterop::PrepareTransfer`.  That blits the AOVs into a slot of
GL-importable textures owned by `HgiInteropVulkan` and makes no GL calls, so it
runs on the render thread.  The main thread later composites the frame with
`HgiInterop::TransferToApp(frame, ...)`, which makes only GL calls.  The copy
pairs are not used, so the copy happens once rather than twice.

`HgiInteropVulkan` keeps a ring of up to three slots, growing from one only
when every slot is in use.  Each slot has two semaphores:

- `vkComplete` is signalled by the blit's submission and waited on by GL
  before the composite reads the slot.
- `glComplete` is signalled by GL once it has finished reading the slot, and
  waited on by the next blit into it.

All the waits are on the GPU.  Neither thread blocks on the other's work, so
the main thread's composite no longer costs a queue submission and a CPU wait
on the render thread's frame.  When the device or driver cannot share memory
between Vulkan and GL, the interop falls back to a CPU readback on the render
thread and an upload on the main thread.

If the main thread composites again before a newer frame arrives, the same
slot is drawn again.  If a newer frame has been prepared in the meantime, the
newest frame is composited instead.

A frame `PrepareTransfer` fails on is dropped, with a warning and a count in
`aovCopyFailures`.  Compositing it unprepared would have the main thread submit
to the render thread's `HgiVulkan`, which `HgiInterop` does not support, and
publishing a handle to the live AOV would have it read while the next frame
overwrites it.

Metal frames are prepared the same way; see [Metal on the render
thread](metalRenderThread.md).  For any other backend, the render thread warns
and drops the frame for the same reason.

## Build requirements

The Vulkan render thread checks `hgiVulkan` for threaded submission, and the
Vulkan route of [GPU buffer sharing](gpuBufferSharing.md) reaches it directly,
but not every USD package exports it.  `FindUSD.cmake` sets
`USD_HAS_HGI_VULKAN` when the `hgiVulkan` target exists; `mayaHydraLib` then
links it and defines `USD_HAS_HGI_VULKAN`, and the Vulkan paths compile out
otherwise.  A build without it still runs the GL path unchanged.

The split interop API is detected from `hgiInterop.h` itself, which defines
`HGIINTEROP_HAS_PREPARE_TRANSFER`.  Against a USD without it, Vulkan frames are
drawn on the main thread, as are frames against a USD whose `HgiVulkan` lacks
threaded submission.

## Secondary-thread submission in USD

Running Vulkan from a render thread also requires USD-side changes, because
`HgiVulkan` rejected submission from any thread but the one that created it.
Those changes make `HgiVulkan` usable from more than one thread:

- Queue submission is serialised by a mutex in `HgiVulkanCommandQueue`, as
  the Vulkan specification requires of a caller sharing a device queue.
  Waits on the timeline semaphore happen outside that mutex.
- Each thread records resource uploads into its own command buffer, and
  submission keeps them in recording order.
- Command buffers can be retired from any thread and are reset by the thread
  that owns their pool.
- `StartFrame`, `EndFrame` and garbage collection may run on any thread.
- `EndFrame` no longer drains the GPU; it caps the frames in flight at
  `HGIVULKAN_MAX_FRAMES_IN_FLIGHT` (default 2).

A build whose USD lacks them reports a coding error from the render thread
rather than drawing.
