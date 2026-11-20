<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Render Thread Pipelining

MayaHydra can draw a frame on a dedicated render thread while the main thread
gets on with producing the next one.  The two overlap instead of running
strictly one after the other, which recovers the GPU time that would otherwise
sit idle while Maya evaluates.

## Structure

`Render()` runs in three stages.  `pushSceneState()` applies scene,
visibility, transform and filter-chain updates; `preparePasses()` builds the
ordered list of passes to draw and configures each one; `_DrawPasses()`
executes them.

The split falls on the boundary between work that must stay on the main thread
and work that can move off it.  `pushSceneState()` and `preparePasses()` write
to scene indices and pass parameters, so they remain on the main thread.
`_DrawPasses()` is what the render thread runs.

## Graphics context ownership

The render thread owns its own graphics context, created in Maya's share group
so both contexts can see the same resources.

Creation happens on the main thread while Maya's context is current, because
that is the only place the share group is reachable.  The context is then made
current on the render thread and stays current there for its entire lifetime.
It is never current on two threads, and never migrates.  Its drawable is a
hidden window that the main thread creates and destroys, because Win32
requires a window to be destroyed on the thread that created it.

The share group shares textures, buffers, programs and sync objects, but not
framebuffers or vertex array objects; those belong to the context that created
them.

HgiGL deletes and recreates a pipeline's vertex array object on whichever
context is current when the pipeline binds.  A pipeline bound on both contexts
would delete, on Maya's context, a name it created on the render thread's, which
on Maya's context may belong to Maya or another plug-in.  So under OpenGL, once
the thread has started, every Hydra draw runs on it: picks run there through
`RunOnRenderThread`, which blocks the main thread until the pick is done, and
the first frame is drawn there and composited as its own frame (see Priming,
below).

`hgi/hgi.h` requires command submission on the "main thread", which for OpenGL
it defines as the thread the context is bound on, so a thread owning its own
context satisfies that contract.  `hgiGL` additionally requires one context
arena per GL context, for the container resources that cannot be shared.

If the context cannot be made current on the thread, `Start()` returns false,
the thread is joined immediately, and drawing stays on the main thread.

The GL render-thread context exists only on Windows.  Under an OpenGL Hgi on
Linux or macOS the thread does not start, and a warning says so.  Vulkan and
Metal need no context.

## Handing frames across

Work crosses to the render thread as a request carrying a frame id, the
colour and depth AOV tokens, the ordered pass list, and the destination
region.  Completed frames come back the same way, carrying the rendered AOV
textures and a GPU fence.

Both directions have a depth of one.  Completed frames are latest wins: if the
render thread publishes before the main thread composites, the older frame is
replaced: an interactive viewport wants the newest frame, not a backlog.

Requests are never replaced or dropped.  Each request's scene push has already
closed a [GPU buffer sharing](gpuBufferSharing.md) producer frame, and under a
Vulkan Hgi that signalled a binary semaphore which only that request's render
waits on; losing the request would signal it twice for one wait.  The interlock
leaves the pending slot empty before the next submit.  A pick, resize or change
of panel lets pending work render before discarding its frame, and stopping the
thread renders a pending request before exiting.

The render thread does not present.  It renders into AOVs and publishes them,
and the main thread composites the most recent completed frame at the start of
its next render operation.  Presentation is disabled on the render thread's
passes because compositing goes through `HgiInterop`, whose GL calls belong to
whichever context the presenting thread owns.  Under Vulkan the render thread
does the GL-free half, copying the frame into the interop's storage, and the
main thread only composites it; see [Vulkan on the render
thread](vulkanRenderThread.md).  Metal does the same, through IOSurfaces
rather than imported memory; see [Metal on the render
thread](metalRenderThread.md).

Colour and depth are both blitted, so Maya's own viewport overlays
depth-compose against the composited scene.

Under an OpenGL Hgi the render thread applies the same GL state around Storm's
passes as the main thread does (`MayaHydraSetRenderGLState`), since its context
does not inherit Maya's.

## Shared vertex buffers

With [GPU buffer sharing](gpuBufferSharing.md) on and an OpenGL Hgi, Storm
cannot draw from VP2's vertex buffers in place once it is on the render
thread's context: VP2 rewrites and recycles them while evaluating the next
frame, concurrently with the render thread's draw.  Starting the thread
switches the bridge to its
[OpenGL copy route](gpuBufferSharing.md#opengl-copy-route), which copies each
changed stream into a buffer MayaHydra owns during the scene push.

The copies run on Maya's context and the draws on the render thread's, so the
request carries a fence signalled after the scene push, which the render
thread waits on before drawing, and the main thread waits on the last render's
fence before the next push overwrites those buffers.  Both waits are queued on
the GPU rather than blocking either thread.

## The scene index interlock

Hydra refuses scene index invalidation while `SyncAll` is running.  The main
thread must therefore wait for the render thread to finish syncing before it
writes to the scene index, which it does by waiting for idle early in
`Render()`, ahead of every scene-index write: `_SetRenderPurposeTags()`, the
registered plugins' `Update()` calls, `FlushPendingUpdates()` and
`pushSceneState()`.

The wait has no timeout.  The first render of a heavy scene can take seconds,
and writing to the scene index during that `SyncAll` is not a race that can be
reported and survived.  After 500 ms it counts the frame in
`idleWaitTimeouts` and keeps waiting.  The wait is reported only with
`TF_DEBUG=MAYAHYDRALIB_RENDER_THREAD`, because the first sync of any heavy
scene takes that long and is not a fault.

Maya callbacks run outside `Render()`, yet adapters dirty, add and remove prims
from their callbacks (transform, visibility and attribute changes, lights,
custom DAG nodes), and Maya's time-change callback sets the frame in the scene
globals.  Those notices reach the render index only through the render thread
capture, which holds them while the render thread may be syncing (see below).

Notices are not the only hazard: an adapter destroyed while the render thread
reads its data sources is one too, since the capture keeps some of them by
reference.  The node-removed callback dropping a deleted
shape's render item override carriers therefore keeps the carriers alive while
the render thread runs (`MayaHydraSceneIndex::SetDeferCallbackRemovals`), and
the next `UpdateRenderItems` drops them.

Overlap therefore covers the part of the draw after sync rather than the whole
draw, and the interlock, not the frame handoff, is what bounds the speedup.

## The render thread capture

Holding back notices is not enough on its own: Hydra's sync and the render
delegate pull prim data through the scene index chain, and much of that chain
reads live data when pulled.  MayaHydra's camera, light, material, DAG mesh,
NURBS curve, custom DAG and image plane adapters read Maya, mayaUsd reads its
USD stage, and Flow Viewport data producers and filters and custom node
translators read whatever they wrap.  Done on the render thread, each of those
reads races the main thread.

`RenderThreadCaptureSceneIndex` closes that gap without touching the render
delegate, so it works with any renderer.  MayaHydra registers it, whenever the
flag is on, as a scene index plugin for every renderer at insertion phase 5000.
That puts it after Storm's and hdsi's plugins and custom node translators, and
before the render delegate's own observers (Flash's is at 9999).  It is a
callback registration rather than a plugin id, because the default hybrid
ordering policy drops a plugin id that has no `loadWithRenderer` entry in
`plugInfo.json`, and such an entry would load `mayaHydraLib` with every renderer
in every session.  `mayaHydraLib` is loaded through Plug, which never unloads a
library, so the callback stays valid after the mayaHydra plugin is unloaded.

The capture passes everything straight through until it is activated, which
happens on the first frame submitted to the render thread.  From then on it:

- keeps a copy of every prim, built from retained data sources.  A dirtied
  prim is re-copied only under its dirty locators; the rest of the previous
  copy is reused.  Multisampled values, such as transforms and points under
  motion blur, are copied as samples over the shutter interval [-1, 1] plus
  the frame itself.  Values of a type Vt does not know, such as `HdTupleType`
  or external GPU buffer handles, and data sources marked
  `RenderThreadCaptureByReference`, such as the lazy readback of a shared GPU
  buffer, are kept by reference;
- is held right before each frame is submitted, after the scene push and pass
  preparation.  Pass preparation runs `FramePass::GetRenderTasks` for every
  pass, which adds and resizes the passes' render buffers and sets their camera
  and task parameters; done only after the hold, the draw would not see those
  changes until the next frame.  With OpenGL, pass preparation runs on the render
  thread while the main thread waits: a pass that reads the previous pass's AOVs
  can copy them into its own buffers, a draw that must not run on Maya's context,
  where it would change GL state Maya tracks.  The draw runs it again right before each pass,
  because a pass reads the AOVs the pass before it has just rendered.  Holding
  copies everything dirtied since the last hold, then
  queues notices from upstream instead of applying them, so the copy cannot
  change while the render thread reads it;
- is released, applying and forwarding the queued notices in order, once the
  render thread is idle: after the interlock, after a discard (pick, resize,
  change of panel), after `-drainRenderThread`, and when the thread stops.

The renderer's own tasks and render buffers are never held, which HVT keeps for
the frame passes in a retained scene index merged directly into the render index,
under `/Pass0`, `/Pass1` and so on.  The passes' camera and lights there are held
like any other prim, since plugins between the two (light linking, flattening)
change them.  When the draw
runs `GetRenderTasks` again and a pass's inputs have changed, as on the first
frames, on a resize or on an offscreen playblast, the pass removes or adds render
buffers, looks them up in the render index right away, and passes the result to
its tasks as parameters.  Held, the render index would still return a removed
buffer, the task would keep its pointer, and the release would delete the buffer
under it.  So notices for those prims go through at once, and the capture keeps
their data sources by reference, since HVT changes them in place.

The capture finds those retained scene indices and their top-level paths on
activation, and takes the prims from them rather than through its input: there
the read would also go through the Flow Viewport scene indices, merged at the
root, while the main thread changes them.  HVT writes its prims on the thread
that draws, between draws, or on the main thread while the render thread is idle,
and the notices come from the thread that wrote them, so this is safe.  No plugin
between the merging scene index and the capture changes task or render buffer
data.  The draw's task parameter changes apply in the same frame, not the next.

A prim's selections are not held either.  The capture serves each copied prim
through a container that reads `selections` from a slot it changes in place
when a notice dirties them, so, as with Flow Viewport's selection scene index, a
prim handed out before a pick shows the new selection right after it.  The rest
of the copy stays fixed, and the notice itself is still held while the render
thread may be syncing, since forwarding it would change the render index's dirty
state under that sync.

A synchronous main-thread frame holds and releases it around the draw, so that
Hydra's parallel sync reads the copy without taking any lock.  A pick run on the
render thread is held the same way.

Between a release and the next hold, a prim read through the capture is copied
on the spot if it is pending.  Flash's observer reads every dirtied prim from
TBB workers while it handles the notice, so those reads take the table lock
shared and only serialize per prim, through a small array of copy locks keyed
by path.  Notices take the table lock exclusively.

The copy is taken on the main thread, so the main thread now does the data
reads that sync used to do on the render thread, for every dirtied prim
whether or not the renderer pulls it.

A container is copied by its `GetNames()`: a name its `Get()` answers but does
not list is missing from the copy, and the renderer silently gets nothing for
it.  MayaHydra's light data sources therefore list every parameter their
adapter serves, in each spelling it accepts (`MayaHydraLightAdapter::
GetLightParamNames()`); a dome light's `textureFile`, for example, used to be
served without being listed.  `MayaHydraDataSource` keeps one list of names per
prim type, which `GetNames()` returns and `Get()` checks first, and the camera
data source serves only the parameters the camera adapter answers.

To catch this at the source, the capture probes each prim it copies in full
(`RenderThreadCaptureSceneIndex::FindUnlistedNames()`): the prim's top level for
the common prim-level schemas, and the containers of the schemas with fixed
members (xform, mesh, basis curves, camera, light, visibility, purpose, extent,
instancer topology, display style) for those members.  Each name served with a
non-empty value but not listed is warned about once per prim type.  Names that
vary per prim, such as primvars and material networks, are not probed, and
neither are retained containers, which list exactly what they hold.  The test
`DataSourceNames.MayaHydraPrimsListEveryName` runs the probe over every prim
MayaHydra produces.

The capture is what keeps Maya callbacks from invalidating the scene index
during the render thread's sync, so overlap always runs with it.

## Priming, discarding and compositing

**The first frame does not overlap.**  There is nothing to composite before a
frame has been produced, so the first frame after resources are initialised
takes the main-thread path, and overlap begins with the second.  Under OpenGL
the first frame is instead drawn on the render thread and composited as its own
frame, as below, so that pipelines bind on one context only.

**Each render op composites the previous op's frame.**  The interlock waits
for that frame before the composite, so the composite never shows anything
older.  In particular, the first redraw after playback stops shows the final
frame of playback, and needs no extra step.

**In-flight work is discarded** when it can no longer be valid: on viewport
resize, where the render target has changed size underneath the frame; on
picking, where a frame rendered before the pick would otherwise be composited
after it and show a pre-pick image; and when the render op is for a different
panel or destination than the last frame was drawn for.  The discard waits,
with no time limit, for any render still running.  After a resize the next
frame is primed again.

**A change of panel composites its own frame.**  One render override draws
every Hydra panel, but the handoff holds a single frame, so a frame drawn for
one panel must not be composited into another.  A render op for a different
panel than the last one discards the handoff and composites its own frame, as
below.  With several Hydra panels visible, every render op does this, so they
do not overlap.

**A time change while stopped composites its own frame.**  Jumping to the
start or end frame, or stepping the time slider, is followed by exactly one
redraw, so compositing the previous frame's image would leave the viewport a
frame behind with nothing to correct it.  When the render override applies a
time change while Maya is not playing, that render op skips the usual
composite, submits its frame, waits for it (`WaitForQuiescence`) and composites
it into Maya's framebuffer, captured at the start of the op.  Such frames do
not overlap, which costs the same as the flag-off path.  Requesting a
catch-up redraw instead does not work reliably, because nothing guarantees
Maya runs it promptly.  The `ownFramesComposited` counter records these
frames.  Such a render op composites only its own frame: the composite is
depth-tested and blended over Maya's framebuffer, so compositing the previous
frame as well would show the animation at both its old and new positions.

## Enabling it

Pipelining is gated behind the `MAYAHYDRA_PIPELINE_OVERLAP` environment
variable and is off by default.  It is read once per process.  Its default
comes from `MAYAHYDRA_PIPELINE_OVERLAP_DEFAULT` in `envSettings.cpp`, which
tests USD for `HGIINTEROP_HAS_PREPARE_TRANSFER`, so that it can be turned on
for a USD that supports it without being turned on for an older one.  It is
off for both for now.

Under Vulkan (USD's `HGI_ENABLE_VULKAN`) the thread starts only if USD
defines `HGIVULKAN_HAS_THREADED_SUBMISSION`, meaning HgiVulkan takes
submissions from several threads, as well as `HGIINTEROP_HAS_PREPARE_TRANSFER`
(`CanRunVulkanRenderThread()`); otherwise MayaHydra warns and renders
synchronously.  Under Metal it needs `HGIINTEROP_HAS_METAL_PREPARE_TRANSFER`
(`CanPrepareMetalFrames()`).

Every overlap-sensitive site is guarded on the flag being set *and* the thread
having actually started, so a failed startup degrades to the original
synchronous path rather than to a half-enabled one.

## Observability

The render thread keeps counters covering frames drawn off the main thread,
frames composited, frames drawn synchronously, render ops that composited
their own frame, discards, slow interlock waits, and each distinct failure
mode: fence signal failures, fence wait timeouts, drain timeouts, presents
skipped or without depth, AOV copy failures, composite fence failures and
shared buffer fence failures.

Two more cover the capture: `primsCaptured`, the prims it copied, and
`mayaReadsOnRenderThread`, the reads MayaHydra guards with `DgAccessLock` that
ran on any thread but the main one while a frame was rendering.  Each of those
is a read of live Maya data the capture should have served.  `captureHolds`,
`captureHoldMicros` and `captureReleaseMicros` give the main-thread cost of
the holds and of the releases that follow them.

They are readable from a test through `mayaHydraTesting -renderThreadStats`,
with `-resetRenderThreadStats` to zero them and `-drainRenderThread` to force
quiescence.

The failure counters exist so that tests can assert nothing silently fell
back.  A frame that quietly reverts to synchronous drawing still produces a
correct image, so a test asserting only on image content would pass while the
feature did nothing.

## Testing

`testRenderThreadOverlap.py` runs only with the flag on.  It asserts that the
thread started, that frames were drawn off the main thread and composited,
that the first frame left an image (drawn synchronously, or under OpenGL
composited as its own frame), and that every failure counter is zero,
`mayaReadsOnRenderThread` included.

Another edits a point light, a spot light, a material, the camera and mesh
points between frames, and asserts that the capture copied prims and that no
Maya read ran while a frame rendered.

Three tests jump the time while stopped (a transform-animated cube, the same
right after playback, and a GPU-deformed skinned mesh under cached playback)
and read Maya's colour buffer after the single redraw, asserting the object
appears only at its new position and that the render op composited its own
frame.

Picking is covered through a `-viewportPick` command that calls Maya's
screen-selection API directly, because scripted selection does not reach the
render override's pick path.  The test asserts both that the object was
selected and that a discard was recorded, so it cannot pass if the discard
stops happening.

## Limitations

- Overlap covers only the portion of the draw after `SyncAll`, as described
  under the interlock above.
- The capture covers only the scene indices before it in the chain.  A render
  delegate's own scene indices after phase 5000, and anything a delegate reads
  outside the scene index, are not covered.
- Upstream containers must list every name they serve.  An unlisted name is
  dropped from the copy without any warning.
- A typed sampled data source must answer `GetValue` with a value of its type.
  An empty value is copied untyped, and a typed read of the copy returns null.
- A producer must update its state before sending the notice for it.  A prim
  read while a notice is handled is copied then, and kept until the next one.
- The copy reads whole prims, so the flattening scene index caches more
  inherited transforms than it would otherwise, and moving an ancestor dirties
  the transform of more descendants, materials included.  A consumer should
  rebuild a prim only for the locators it reads.
- Data sources kept by reference are read live on the render thread.  The lazy
  GPU buffer readback reads a buffer Maya's copy has already landed in (see
  shared vertex buffers above); a value of a type Vt does not know comes from a
  retained schema builder in practice, but nothing enforces that.
- A renderer that asks for samples outside the shutter interval [-1, 1] gets
  the nearest captured sample.
- Composited overlays sort against the scene as of the last completed frame,
  which is one frame behind the main thread's current state.
- Time changes outside playback and changes of panel composite their own
  frame; other single-redraw changes (for example a scripted attribute edit
  with no further redraw) still show the previous frame until the next redraw.
