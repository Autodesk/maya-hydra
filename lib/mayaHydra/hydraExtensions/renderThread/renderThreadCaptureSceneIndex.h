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

#ifndef MAYAHYDRA_RENDER_THREAD_CAPTURE_SCENE_INDEX_H
#define MAYAHYDRA_RENDER_THREAD_CAPTURE_SCENE_INDEX_H

// Serves the render thread a copy of the scene taken on the main thread.
//
// While the render thread draws, Hydra's sync and the render delegate pull prim
// data through the scene index chain. Upstream of the render delegate, that
// chain holds MayaHydraSceneIndex's adapters, mayaUsd, Flow Viewport data
// producers and filters, and custom node translators, many of which read Maya
// or a USD stage when pulled, while the main thread may be changing them.
//
// This scene index sits in each render index's plugin chain after all of those
// (kInsertionPhase) and before the render delegate's own consumers. Once active,
// it keeps a copy of every prim built from retained data sources, reads its
// input only on the main thread, and holds back notices while the render thread
// may be reading, so the copy cannot change underneath it.
//
// The renderer's own tasks and render buffers are the exception, which HVT keeps for
// its frame passes in a retained scene index merged directly into the render index.
// HVT changes them while it draws, between passes, and looks them up in the render
// index right after, so their notices go through at once, even while holding. The capture keeps their data sources by reference, as
// HVT changes them in place, taken from that retained scene index rather than
// through its input, which would also reach the scene indices the main thread is
// changing.
//
// A prim's selections are not held either: like Flow Viewport's selection scene
// index, every prim it has handed out shows a selection change at once. The rest
// of the copy stays fixed, and the notice is still held.

#include <mayaHydraLib/api.h>

#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/tf/weakPtr.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndexObserver.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/usd/sdf/pathTable.h>

#include <atomic>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <variant>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

class RenderThreadCaptureSceneIndex;
using RenderThreadCaptureSceneIndexRefPtr = PXR_NS::TfRefPtr<RenderThreadCaptureSceneIndex>;

/// Marks a data source the capture keeps by reference rather than copying,
/// because copying it would force work the renderer may never ask for, such as
/// a GPU readback. Such a data source must be safe to read on the render thread.
class MAYAHYDRALIB_API RenderThreadCaptureByReference
{
public:
    virtual ~RenderThreadCaptureByReference();
};

class MAYAHYDRALIB_API RenderThreadCaptureSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
{
public:
    /// After every plugin that may read Maya or USD (Storm's and hdsi's go up to
    /// 1000, custom node translators use 0) and before Flash's observer (9999).
    static constexpr int kInsertionPhase = 5000;

    static RenderThreadCaptureSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& input);

    /// The capture scene index in \p renderIndex's chain, or null.
    static RenderThreadCaptureSceneIndex* Find(PXR_NS::HdRenderIndex* renderIndex);

    /// The names \p primSource serves but does not list in GetNames(), relative to
    /// the prim. The copy takes a container's names from GetNames(), so it would
    /// drop each of these. Probes the top level for the common prim-level schemas,
    /// and the containers of those with fixed members (xform, mesh, camera, light,
    /// ...) for those members. A name served with an empty value does not count.
    /// The capture warns about these when it first copies a prim.
    static std::vector<PXR_NS::HdDataSourceLocator>
    FindUnlistedNames(const PXR_NS::HdContainerDataSourceHandle& primSource);

    /// Main thread. Until this is called everything passes straight through, so
    /// render indices that never meet a render thread pay nothing.
    void Activate();
    bool IsActive() const { return _active.load(std::memory_order_acquire); }

    /// Main thread, before a frame is submitted: bring the copy up to date, then
    /// hold back notices so it stays fixed while the render thread reads it.
    void Hold();

    /// Main thread, once the render thread is idle: apply the notices held back
    /// and forward them.
    void Release();

    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;
    PXR_NS::SdfPathVector    GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

protected:
    explicit RenderThreadCaptureSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& input);

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;

private:
    // The prim's selections, changed in place, and the copy that serves them.
    struct _Selections;
    class _PrimSource;

    struct _Entry
    {
        // The copy behind a _PrimSource, or the renderer's own prim.
        PXR_NS::HdSceneIndexPrim            prim;
        PXR_NS::HdContainerDataSourceHandle copy;
        std::shared_ptr<_Selections>        selections;
        // What changed since the copy was taken. Ignored until the first copy.
        PXR_NS::HdDataSourceLocatorSet dirty;
        bool                           copied = false;
        bool                           pending = false;
        // Created only as the ancestor of an added prim. The input may stop
        // listing it once it has no descendants left, so neither does this.
        bool implicit = false;
    };
    using _Table = PXR_NS::SdfPathTable<_Entry>;

    using _Notice = std::variant<
        PXR_NS::HdSceneIndexObserver::AddedPrimEntries,
        PXR_NS::HdSceneIndexObserver::RemovedPrimEntries,
        PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries>;

    // All under an exclusive lock on _mutex.
    void _Apply(const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries);
    void _Apply(const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries);
    void _Apply(const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries);
    void _Insert(const PXR_NS::SdfPath& path);
    void _MarkPending(_Entry& entry, const PXR_NS::SdfPath& path);
    void _CopyPending();

    // Under an exclusive lock, or a shared one plus _CopyLock(path).
    void _Copy(const PXR_NS::SdfPath& path, _Entry& entry) const;
    std::mutex& _CopyLock(const PXR_NS::SdfPath& path) const;

    void _Forward(const _Notice& notice);

    // Under an exclusive lock, on the main thread: give copied prims their new
    // selections at once, even while holding.
    void _UpdateSelections(const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries);

    // Sends the entries for the renderer's own prims on at once, after
    // storeRenderer(those entries) under an exclusive lock, and holds or applies
    // the others.
    template <class Entries, class StoreRenderer, class Send>
    void _Receive(const Entries& entries, StoreRenderer storeRenderer, Send send);

    // Whether an entry is for one of the renderer's own tasks or render buffers,
    // which are under one of _rendererRoots. Under an exclusive lock.
    bool _IsRendererPrim(const PXR_NS::SdfPath& path, const PXR_NS::TfToken& primType) const;
    bool _IsRendererPrim(const PXR_NS::HdSceneIndexObserver::AddedPrimEntry& entry) const;
    bool _IsRendererPrim(const PXR_NS::HdSceneIndexObserver::RemovedPrimEntry& entry) const;
    bool _IsRendererPrim(const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntry& entry) const;
    // The prim at \p path in the renderer's retained scene indices, if under one of
    // _rendererRoots. The renderer writes its prims on the thread that draws,
    // between draws, or on the main thread while that thread is idle, and its
    // notices come from the thread that wrote them, so reading them then is safe.
    PXR_NS::HdSceneIndexPrim _RendererSourcePrim(const PXR_NS::SdfPath& path) const;
    // Under an exclusive lock: keep the renderer's prim by reference.
    void _StoreRendererPrim(const PXR_NS::SdfPath& path);
    // Main thread, while the render thread is idle.
    void _FindRendererSources();

    // The retained scene indices merged directly into the render index, where HVT
    // keeps the renderer's prims, and their top-level paths. Not those merged
    // further upstream, which the main thread writes.
    std::vector<PXR_NS::TfWeakPtr<PXR_NS::HdRetainedSceneIndex>> _rendererSources;
    PXR_NS::SdfPathVector                                        _rendererRoots;

    std::atomic<bool> _active { false };
    std::atomic<bool> _holding { false };

    // Guards the table and the queue. Not taken while holding: then nothing writes
    // and every reader only reads. Otherwise notices take it exclusively and
    // readers shared, since a render delegate may read the prims it is notified
    // of from many threads at once (Flash's observer does). A reader copying a
    // pending prim also takes that prim's copy lock.
    mutable std::shared_mutex _mutex;
    mutable std::mutex        _copyLocks[64];
    mutable _Table            _table;
    std::vector<PXR_NS::SdfPath> _pending;
    std::vector<_Notice>         _held;
};

/// Register the capture scene index for every renderer. Main thread, before any
/// render index is created; call once per process, only when the render thread
/// may be used.
MAYAHYDRALIB_API
void RegisterRenderThreadCaptureSceneIndexPlugin();

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_RENDER_THREAD_CAPTURE_SCENE_INDEX_H
