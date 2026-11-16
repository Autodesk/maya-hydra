<!-- Copyright 2026 Autodesk, Inc. All rights reserved. -->

# Frame Publication Boundary

MayaHydra names each frame the main thread produces and marks the point at
which that frame is complete.  A consumer that draws asynchronously can then
tell which frame's scene state it is about to render, and whether the main
thread has since moved on to a newer one.

Nothing here assumes a render thread, and all of the behaviour below is
testable without one.

## What it carries

The boundary carries frame identity only; it stages and copies no scene data.
Translated scene data already reaches a consumer through the scene index.  What
a consumer cannot recover from the scene index is *which* frame the state it is
looking at belongs to, and a monotonic frame id supplies exactly that.

## The per-frame sequence

`SnapshotManager` is a singleton, so the flush hooks can reach it without
threading a handle through the render override.  Each frame runs two calls,
both on the main thread:

1. `AdvanceFrame()` at the start of a flush, before any of the frame's work is
   produced.  It increments the frame id and returns the new value.
2. `PublishBarrier()` once the flush has finished, marking that frame id as
   published.

`GetPublishedFrameId()` reports the most recent frame the main thread finished,
and `GetCurrentFrameId()` the one in progress.

Publishing is a release store and reading is an acquire load, so a consumer
that observes a new published frame id also sees everything the main thread
produced for that frame.  Without that ordering, a consumer could see a fresh
frame id while still holding stale scene state.

`PublishBarrier()` is safe to call more than once per frame.  Republishing a
frame id that is already published is skipped after a single relaxed read, so
every site that might turn out to be the last one in a frame can simply call
it, rather than the code having to identify which site that is.

Frame 0 is never published: it is the "no frame produced yet" value a consumer
sees before the first flush.

## Latest wins, with an in-flight depth of one

If the producer outruns the consumer, intermediate frames are skipped rather
than queued.  The barrier overwrites the published frame id, and a consumer
arriving between two barriers picks up the most recent one.  An interactive
viewport that has fallen behind wants the newest frame, not a backlog it must
work through before catching up.

## Enabling it

The boundary is gated behind the `MAYAHYDRA_SNAPSHOT` environment variable and
is off by default.  The flag is read once at startup and cached.

When it is off, `AdvanceFrame()` returns 0, `PublishBarrier()` does nothing,
and the published frame id stays 0, so the cost is one predictable branch per
call site.  Enabling render-thread pipelining implies this flag, since
overlapped drawing cannot identify its frame without it.

## Observability

`mayaHydraTesting -snapshotStats` reports whether the boundary is enabled, the
current frame id, the published frame id, and the number of barriers that
actually published.  The publish count matters separately from the ids: it
distinguishes a boundary that is advancing and publishing normally from one
that is advancing while every publish is skipped.

## Invariants

Two properties hold whenever the boundary is enabled, and both are asserted by
`testFramePublication.py`:

- The published frame id never leads the current frame id.  A frame cannot be
  published before it has been named.
- Frame ids advance monotonically, and the publish count rises with them.

The test is registered twice: once with the flag off, to confirm the boundary
is genuinely inert, and once with it on, to confirm the invariants above.
