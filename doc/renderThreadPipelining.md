<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Render Thread Pipelining

MayaHydra can draw a frame on a dedicated render thread while the main thread
gets on with producing the next one.  The two overlap instead of running
strictly one after the other, which recovers the GPU time that would otherwise
sit idle while Maya evaluates.

This builds on the [frame publication boundary](framePublication.md), which
supplies the frame id that identifies the scene state a given draw belongs to.
Enabling pipelining enables that boundary.

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
It is never current on two threads, and never migrates.

`hgi/hgi.h` requires command submission on the "main thread", which for OpenGL
it defines as the thread the context is bound on, so a thread owning its own
context satisfies that contract.  `hgiGL` additionally requires one context
arena per GL context, for the container resources that cannot be shared.

If the context cannot be made current on the thread, `Start()` returns false,
the thread is joined immediately, and drawing stays on the main thread.

## Handing frames across

Work crosses to the render thread as a request carrying the frame id, the
colour and depth AOV tokens, the ordered pass list, and the destination
region.  Completed frames come back the same way, carrying the rendered AOV
textures and a GPU fence.

Both directions have a depth of one.  Completed frames are latest wins: if the
render thread publishes before the main thread composites, the older frame is
replaced, for the same reason as the publication boundary: an interactive
viewport wants the newest frame, not a backlog.

Requests are never replaced or dropped.  Each request's scene push has already
closed a [GPU buffer sharing](gpuBufferSharing.md) producer frame, and under a
Vulkan Hgi that signalled a binary semaphore which only that request's render
waits on; losing the request would signal it twice for one wait.  The interlock
leaves the pending slot empty before the next submit.  A pick or resize lets pending work render before
discarding its frame, and stopping the thread renders a pending request before
exiting.

The render thread does not present.  It renders into AOVs and publishes them,
and the main thread composites the most recent completed frame at the start of
its next render operation.  Presentation is disabled on the render thread's
passes because compositing goes through `HgiInterop`, whose GL calls belong to
whichever context the presenting thread owns.  Under Vulkan the render thread
does the GL-free half, copying the frame into the interop's storage, and the
main thread only composites it; see [Vulkan on the render
thread](vulkanRenderThread.md).

Colour and depth are both blitted, so Maya's own viewport overlays
depth-compose against the composited scene.

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
reported and survived.  After 500 ms it warns that it is still waiting and
counts the frame in `idleWaitTimeouts`, then keeps waiting.

Maya callbacks run outside `Render()`, so they must not write to the scene index
either, yet adapters dirty, add and remove prims from their callbacks (transform,
visibility and attribute changes, lights, custom DAG nodes).  `MayaHydraSceneIndex`
therefore queues `AddPrims`, `RemovePrims` and `DirtyPrims` from submitting a
frame until the next interlock (`SetRenderThreadBusy`): in call order, with
repeated dirties of a prim merged into one entry.  The interlock applies the
queue right after the wait.  Queued changes are not visible to queries on the
scene index until then.

Notices are not the only hazard: an adapter destroyed while the render thread
reads its data sources is one too.  The node-removed callback dropping a deleted
shape's render item override carriers therefore keeps the carriers alive while
the render thread runs (`MayaHydraSceneIndex::SetDeferCallbackRemovals`), and
the next `UpdateRenderItems` drops them.

Overlap therefore covers the part of the draw after sync rather than the whole
draw, and the interlock, not the frame handoff, is what bounds the speedup.
Removing it would require the render thread to read an immutable per-frame copy
of the scene instead of the live scene index.

## Deferred time changes

Maya's time-change callback fires from the event loop and can land while the
render thread is inside `SyncAll`.  Setting the frame invalidates the scene
index, which Hydra refuses during a sync.

While the render thread is running, the callback therefore records the new
frame time instead of applying it, and `Render()` applies it past the
interlock.  The camera refresh travels with that deferred apply: cameras are
re-evaluated and dirtied at the same point, because the callback returned
without touching the scene index.  Cameras need this explicitly, since Maya's
per-node world-matrix callbacks do not fire reliably under the Evaluation
Manager during playback.

When the render thread is not running, the callback applies the time and
refreshes cameras immediately, so behaviour with the feature disabled is
unchanged.

## Priming, discarding and settling

**The first frame is drawn synchronously.**  There is nothing to composite
before a frame has been produced, so the first frame after resources are
initialised takes the main-thread path, and overlap begins with the second.

**In-flight work is discarded** when it can no longer be valid: on viewport
resize, where the render target has changed size underneath the frame, and on
picking, where a frame rendered before the pick would otherwise be composited
after it and show a pre-pick image.  After a discard the next frame primes
synchronously again.

**Playback stop is settled explicitly.**  When playback stops, the last
overlapped frame may still be in flight, so the viewport would otherwise show
the second-to-last frame indefinitely.  A settle step detects the transition
and schedules redraws until the final frame has been composited, up to
`kMaxSettleRetries`.  The retry is self-scheduling, so that bound is what
keeps a thread which never produces the frame from spinning the viewport.

**A time change while stopped composites its own frame.**  Jumping to the
start or end frame, or stepping the time slider, is followed by exactly one
redraw, so compositing the previous frame's image would leave the viewport a
frame behind with nothing to correct it.  When the render override applies a
time change while Maya is not playing, that render op skips the usual
composite, submits its frame, waits for it (`WaitForQuiescence`) and composites
it into Maya's framebuffer, captured at the start of the op.  Such frames do
not overlap, which costs the same as the flag-off path.  Requesting a
catch-up redraw instead does not work reliably, because nothing guarantees
Maya runs it promptly.  The `timeChangesComposited` counter records these
frames.  Such a render op also cancels a pending settle: the composite is
depth-tested and blended over Maya's framebuffer, so compositing the settled
frame as well, as happens when the jump is the first redraw after playback
stops, would show the animation at both its old and new positions.

## Enabling it

Pipelining is gated behind the `MAYAHYDRA_PIPELINE_OVERLAP` environment
variable and is off by default.  Setting it also enables the frame publication
boundary, since overlapped drawing cannot identify its frame without one.

Every overlap-sensitive site is guarded on the flag being set *and* the thread
having actually started, so a failed startup degrades to the original
synchronous path rather than to a half-enabled one.

## Observability

The render thread keeps counters covering frames drawn off the main thread,
frames composited, frames drawn synchronously, discards, quiescence waits,
settles and settle retries, and each distinct failure mode: fence signal
failures, fence wait timeouts, drain timeouts, presents skipped or without
depth, AOV copy failures, copy-target wait failures and shared buffer fence
failures.

They are readable from a test through `mayaHydraTesting -renderThreadStats`,
with `-resetRenderThreadStats` to zero them and `-drainRenderThread` to force
quiescence.

The failure counters exist so that tests can assert nothing silently fell
back.  A frame that quietly reverts to synchronous drawing still produces a
correct image, so a test asserting only on image content would pass while the
feature did nothing.

## Testing

`testRenderThreadOverlap.py` is registered twice, once with the flag off and
once with it on.

With the flag off it asserts every counter is zero, which is what proves the
feature is genuinely inert rather than merely harmless.  With it on it asserts
that the thread started, that frames were drawn off the main thread and
composited, that the first frame was drawn synchronously, and that every
failure counter is zero.

Picking is covered through a `-viewportPick` command that calls Maya's
screen-selection API directly, because scripted selection does not reach the
render override's pick path.  The test asserts both that the object was
selected and that a discard was recorded, so it cannot pass if the discard
stops happening.

## Limitations

- Overlap covers only the portion of the draw after `SyncAll`, as described
  under the interlock above.
- Only `MayaHydraSceneIndex` queues its notices.  Other scene indices that
  change from a Maya callback rather than from `Render()` are not covered.
- An adapter that changes its cached data from a callback, rather than only
  sending a notice, can still be read mid-change by a sync on the render thread
  (for example a sync that pulls some attributes from the old data and some from
  the new).  Such adapters need a commit step that runs behind the interlock.
- Composited overlays sort against the scene as of the last completed frame,
  which is one frame behind the main thread's current state.
- The settle step triggers on a playing-to-stopped transition, and time
  changes outside playback composite their own frame; other single-redraw
  changes (for example a scripted attribute edit with no further redraw) still
  show the previous frame until the next redraw.
