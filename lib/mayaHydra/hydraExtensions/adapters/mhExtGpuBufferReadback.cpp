//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include "mhExtGpuBufferReadback.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/profilingUtils.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/tf/envSetting.h>
#include <pxr/base/tf/hash.h>

#include <algorithm>
#include <atomic>
#include <mutex>
#include <unordered_map>
#include <utility>
#include <vector>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(MAYAHYDRA_GPU_BUFFER_READBACK_PREFETCH, true,
    "Prefetch the shared VP2 streams a CPU consumer reads into persistently "
    "mapped staging memory, batching their readback into one GPU sync per "
    "frame. Off reads each stream back on demand.");

#define TF_DEBUG_EXT_GPU_BUFFER_READBACK(fmt, ...)                      \
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING)         \
        .Msg("[GPU Buffer Sharing]: " fmt, ##__VA_ARGS__)

namespace MhExtGpuBufferReadback {

// One persistently mapped block of staging memory that prefetch copies land
// in. Created, filled and released on the main thread; workers only read
// through `mapped`, which is valid on any thread.
struct _StagingChunk
{
    GLuint               buffer = 0;
    size_t               capacity = 0;
    const unsigned char* mapped = nullptr;
};

// Built once per renderer, on the first publish, and dropped only in Shutdown.
// Maya renders every VP2 panel with one HGLRC -- the hidden resource window's
// context, made current with each panel's DC -- and never replaces it while
// running: OGS's GL ResetDevice is unimplemented and a rendering-engine change
// needs a restart. Panels that render with their own context (stereo) share
// objects with it, so one readback context serves them all.
//
// Destroyed on the main thread only (see _ReapRetiredContexts), because the
// shared context must be destroyed on the thread that created it.
struct _ReadbackContext
{
    // The hidden context readbacks run on, and the Maya context it was built
    // to share with.
    std::unique_ptr<MAYAHYDRA_NS::MhSharedGLContext> gl;
    void*                                          mayaContext = nullptr;

    // Fenced on Maya's context at the end of the producer's frame; every
    // lease waits on it before reading.
    std::atomic<GLsync> frameFence { nullptr };

    // Prefetch staging, refilled from the start every frame. Main thread only.
    // Chunks are kept across frames, so the memory held is the peak per-frame
    // prefetch volume.
    std::vector<_StagingChunk> staging;
    size_t                     stagingChunk = 0;
    size_t                     stagingOffset = 0;
};

namespace {

// Held only to copy or swap the pointer, never across GL work.
std::mutex             _readbackContextMutex;
std::shared_ptr<_ReadbackContext> _currentReadbackContext;

// Whether this renderer has tried to build its readback context, successfully
// or not. A failure is not retried before Shutdown: whatever made it fail is
// still there.
std::atomic<bool> _captureAttempted { false };

// The Maya context the first attempt saw, and whether a different one has
// been reported since. Main thread only, like Capture().
void* _capturedMayaContext = nullptr;
bool  _contextChangeWarned = false;

// Readback contexts dropped by Shutdown while a Lease still held them. Main
// thread only.
std::vector<std::shared_ptr<_ReadbackContext>> _retiredContexts;

// One thread reads back at a time. Held for the whole lease: it is what keeps
// the readback context current on one thread only.
std::mutex _readbackMutex;

struct _Entry
{
    std::mutex mutex;
    bool       done = false;
    VtValue    value;

    // Set by Prefetch: where this frame's copy of the span lands, how many
    // bytes it is, and the readback context whose staging that is. A context
    // rebuilt since makes the pointer unusable.
    const unsigned char*    staged = nullptr;
    size_t                  stagedBytes = 0;
    const _ReadbackContext* stagedContext = nullptr;
};

struct _ReadKeyHash
{
    size_t operator()(const ReadKey& key) const
    {
        return TfHash::Combine(
            key.rawHandle,
            key.byteOffset,
            key.byteStride,
            key.numElements,
            key.elementType.hash_code());
    }
};

// Held only for find-or-insert, never across a readback.
std::mutex _cacheMutex;
std::unordered_map<ReadKey, std::shared_ptr<_Entry>, _ReadKeyHash> _cache;
std::atomic<size_t> _cacheHits { 0 };
std::atomic<size_t> _prefetchedReads { 0 };
std::atomic<size_t> _fetchedReads { 0 };
std::atomic<size_t> _prefetchCopies { 0 };

// Whether this frame's prefetch copies are known complete. Reset in
// BeginFrame; latched by the first pull that waits on the frame fence.
enum class _FenceState { kPending, kReady, kFailed };
std::atomic<_FenceState> _prefetchFenceState { _FenceState::kPending };
std::mutex               _prefetchFenceMutex;

constexpr size_t _StagingChunkBytes = size_t(32) << 20;
constexpr size_t _StagingAlignment = 64;

// Whether a producer frame has been opened since the last Shutdown. Without
// one nothing clears the cache, so it is bypassed rather than allowed to go
// stale.
std::atomic<bool> _frameBegan { false };

// Whether any consumer has pulled a CPU value since the last Shutdown. Until
// one has, nothing reads back and nothing is prefetched, so EndFrame skips the
// fence and its flush: a session whose consumers all take extGpuBuffer pays
// nothing per frame. Set by the first pull, so that pull's own frame is the one
// frame whose on-demand readbacks are not ordered after VP2's writes by the
// fence, only implicitly.
std::atomic<bool> _cpuConsumerSeen { false };

std::shared_ptr<_ReadbackContext> _GetReadbackContext()
{
    std::lock_guard<std::mutex> lock(_readbackContextMutex);
    return _currentReadbackContext;
}

void* _GetCurrentContext()
{
    return MAYAHYDRA_NS::MhSharedGLContext::GetCurrentNativeContext();
}

bool _PrefetchEnabled()
{
    static const bool enabled = TfGetEnvSetting(MAYAHYDRA_GPU_BUFFER_READBACK_PREFETCH);
    return enabled;
}

// Release a readback context's staging. With \p deleteGl the buffers are
// unmapped and deleted, which needs a context in their share group current;
// without it the names are only forgotten, for a share group that is already
// gone.
void _ReleaseStaging(_ReadbackContext& readback, bool deleteGl)
{
    for (const _StagingChunk& chunk : readback.staging) {
        if (deleteGl && chunk.buffer) {
            if (chunk.mapped && glUnmapNamedBuffer) {
                glUnmapNamedBuffer(chunk.buffer);
            }
            if (glDeleteBuffers) {
                glDeleteBuffers(1, &chunk.buffer);
            }
        }
    }
    readback.staging.clear();
    readback.stagingChunk = 0;
    readback.stagingOffset = 0;
}

// Reserve \p bytes of staging in \p readback for this frame. Main thread,
// Maya's context current. Returns null when no staging could be had.
const unsigned char* _AllocateStaging(
    _ReadbackContext& readback,
    size_t  bytes,
    GLuint* outBuffer,
    size_t* outOffset)
{
    while (readback.stagingChunk < readback.staging.size()) {
        const _StagingChunk& chunk = readback.staging[readback.stagingChunk];
        const size_t offset = (readback.stagingOffset + _StagingAlignment - 1)
            & ~(_StagingAlignment - 1);
        if (offset + bytes <= chunk.capacity) {
            readback.stagingOffset = offset + bytes;
            *outBuffer = chunk.buffer;
            *outOffset = offset;
            return chunk.mapped + offset;
        }
        ++readback.stagingChunk;
        readback.stagingOffset = 0;
    }

    if (!glCreateBuffers || !glNamedBufferStorage || !glMapNamedBufferRange) {
        return nullptr;
    }
    // A stream larger than a chunk gets a chunk of its own. Appending never
    // moves existing chunks' memory, so pointers already handed out stay
    // valid.
    const size_t capacity = std::max(_StagingChunkBytes, bytes);
    GLuint buffer = 0;
    glCreateBuffers(1, &buffer);
    if (!buffer) {
        return nullptr;
    }
    const GLbitfield storageFlags = GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT
        | GL_MAP_COHERENT_BIT | GL_CLIENT_STORAGE_BIT;
    glNamedBufferStorage(
        buffer, static_cast<GLsizeiptr>(capacity), nullptr, storageFlags);
    void* const mapped = glMapNamedBufferRange(
        buffer, 0, static_cast<GLsizeiptr>(capacity),
        GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT);
    if (!mapped) {
        glDeleteBuffers(1, &buffer);
        static std::once_flag warned;
        std::call_once(warned, []() {
            TF_WARN(
                "Could not create persistently mapped staging for GPU "
                "buffer readback prefetch; reading back on demand");
        });
        return nullptr;
    }
    readback.staging.push_back(
        { buffer, capacity, static_cast<const unsigned char*>(mapped) });
    readback.stagingChunk = readback.staging.size() - 1;
    readback.stagingOffset = bytes;
    TF_DEBUG_EXT_GPU_BUFFER_READBACK(
        "readback staging grown to %zu chunks (%zu MB this chunk)\n",
        readback.staging.size(), capacity >> 20);
    *outBuffer = buffer;
    *outOffset = 0;
    return readback.staging.back().mapped;
}

// Destroy retired readback contexts no Lease holds any more. A retired one
// cannot gain a reference -- leases copy only _currentReadbackContext -- so
// use_count() == 1 is final.
void _ReapRetiredContexts()
{
    _retiredContexts.erase(
        std::remove_if(
            _retiredContexts.begin(),
            _retiredContexts.end(),
            [](const std::shared_ptr<_ReadbackContext>& readback) {
                return readback.use_count() == 1;
            }),
        _retiredContexts.end());
}

// Build the readback context, sharing with \p mayaContext, current on the
// calling thread. On failure none is installed and readback stays off until
// Shutdown.
void _BuildReadbackContext(void* mayaContext)
{
    std::unique_ptr<MAYAHYDRA_NS::MhSharedGLContext> gl
        = MAYAHYDRA_NS::MhSharedGLContext::CreateFromCurrent();
    if (!gl) {
        TF_RUNTIME_ERROR(
            "Could not create a GL context sharing Maya's resources; "
            "lazy readback of shared VP2 buffers is disabled until the "
            "renderer restarts");
        return;
    }
    auto readback = std::make_shared<_ReadbackContext>();
    readback->gl = std::move(gl);
    readback->mayaContext = mayaContext;
    {
        std::lock_guard<std::mutex> lock(_readbackContextMutex);
        _currentReadbackContext = std::move(readback);
    }
    TF_DEBUG_EXT_GPU_BUFFER_READBACK(
        "readback context created for Maya context %p\n", mayaContext);
}

} // namespace

void Capture()
{
    void* const mayaContext = _GetCurrentContext();
    if (_captureAttempted.load(std::memory_order_acquire)) {
        // Maya is not expected to publish under another context (see
        // _ReadbackContext). Reported once per renderer, in case it ever does.
        if (mayaContext && mayaContext != _capturedMayaContext && !_contextChangeWarned) {
            _contextChangeWarned = true;
            TF_WARN(
                "Maya's GL context changed from %p to %p since the GPU buffer "
                "readback context was built; readback keeps sharing with the "
                "first one. Expected only for stereo panels, whose context is in "
                "the same share group; otherwise lazy CPU readback may fail",
                _capturedMayaContext, mayaContext);
        }
        return;
    }
    if (!mayaContext) {
        return; // tried again at the next publish
    }
    _captureAttempted.store(true, std::memory_order_release);
    _capturedMayaContext = mayaContext;
    _BuildReadbackContext(mayaContext);
}

void BeginFrame()
{
    _frameBegan.store(true, std::memory_order_release);

    const size_t hits = _cacheHits.exchange(0);
    const size_t prefetched = _prefetchedReads.exchange(0);
    const size_t fetched = _fetchedReads.exchange(0);
    const size_t copies = _prefetchCopies.exchange(0);
    if (hits || prefetched || fetched || copies) {
        TF_DEBUG_EXT_GPU_BUFFER_READBACK(
            "readback last frame: %zu prefetched, %zu fetched, %zu hits "
            "(%zu prefetch copies issued)\n",
            prefetched, fetched, hits, copies);
    }

    // Before anything is published this frame, so a value read before VP2
    // rewrote a buffer in place can never be returned after it.
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        _cache.clear();
    }

    _prefetchFenceState.store(_FenceState::kPending, std::memory_order_release);

    if (const std::shared_ptr<_ReadbackContext> readback = _GetReadbackContext()) {
        // Every entry pointing into staging was just dropped, so it can be
        // refilled from the start.
        readback->stagingChunk = 0;
        readback->stagingOffset = 0;

        // Sync objects are shared, and every context current during a
        // producer frame is in Maya's one share group, so any will do.
        const GLsync fence = readback->frameFence.exchange(nullptr);
        if (fence && glDeleteSync && _GetCurrentContext()) {
            glDeleteSync(fence);
        }
    }

    _ReapRetiredContexts();
}

void EndFrame()
{
    if (!_cpuConsumerSeen.load(std::memory_order_acquire)) {
        return;
    }
    const std::shared_ptr<_ReadbackContext> readback = _GetReadbackContext();
    if (!readback || !glFenceSync) {
        return;
    }
    // The fence belongs in Maya's command stream, after VP2's writes.
    if (_GetCurrentContext() != readback->mayaContext) {
        return;
    }
    const GLsync fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
    // Required: waiting in another context on a fence that was never flushed
    // can wait for ever.
    glFlush();
    if (const GLsync stale = readback->frameFence.exchange(fence)) {
        if (glDeleteSync) {
            glDeleteSync(stale);
        }
    }
}

void Shutdown()
{
    {
        std::lock_guard<std::mutex> lock(_cacheMutex);
        _cache.clear();
    }

    std::shared_ptr<_ReadbackContext> readback;
    {
        std::lock_guard<std::mutex> lock(_readbackContextMutex);
        readback = std::move(_currentReadbackContext);
    }
    if (readback) {
        // Fence and staging buffers are shared objects; any context in
        // Maya's share group can delete them.
        const bool shareGroupCurrent = _GetCurrentContext() != nullptr;
        if (const GLsync fence = readback->frameFence.exchange(nullptr)) {
            if (glDeleteSync && shareGroupCurrent) {
                glDeleteSync(fence);
            }
        }
        _ReleaseStaging(*readback, shareGroupCurrent);
        _retiredContexts.push_back(std::move(readback));
    }
    _ReapRetiredContexts();
    if (!_retiredContexts.empty()) {
        TF_WARN(
            "%zu GL readback context(s) still leased at shutdown",
            _retiredContexts.size());
    }

    // The next renderer builds a fresh readback context.
    _captureAttempted.store(false, std::memory_order_release);
    _capturedMayaContext = nullptr;
    _contextChangeWarned = false;

    // Nothing clears the cache until a new renderer opens its first frame, so
    // pulls bypass it until then rather than fill it with values that would
    // never be dropped.
    _frameBegan.store(false, std::memory_order_release);

    // The next renderer may have no CPU consumer at all.
    _cpuConsumerSeen.store(false, std::memory_order_release);
}

namespace {

// Wait, once per frame, for the GPU to finish this frame's prefetch copies.
// Returns false when they cannot be relied on; the caller then reads back on
// demand instead.
bool _WaitPrefetchFence()
{
    if (_prefetchFenceState.load(std::memory_order_acquire)
        == _FenceState::kReady) {
        return true;
    }
    std::lock_guard<std::mutex> lock(_prefetchFenceMutex);
    const _FenceState state =
        _prefetchFenceState.load(std::memory_order_acquire);
    if (state != _FenceState::kPending) {
        return state == _FenceState::kReady;
    }

    const std::shared_ptr<_ReadbackContext> readback = _GetReadbackContext();
    const GLsync fence = readback ? readback->frameFence.load() : nullptr;
    if (!fence) {
        // A pull before the producer's frame closed: the copies are not
        // fenced yet. Not latched, since the fence comes later this frame.
        return false;
    }

    // glClientWaitSync needs a context in the share group current.
    Lease lease;
    if (!lease || !glClientWaitSync) {
        _prefetchFenceState.store(_FenceState::kFailed, std::memory_order_release);
        return false;
    }
    // Bounded: a fence that never signals must cost a slow frame, not a hang.
    constexpr GLuint64 SliceNs = 1000000; // 1 ms
    constexpr int      MaxSlices = 5000;  // 5 s
    for (int i = 0; i < MaxSlices; ++i) {
        const GLenum result =
            glClientWaitSync(fence, GL_SYNC_FLUSH_COMMANDS_BIT, SliceNs);
        if (result == GL_ALREADY_SIGNALED
            || result == GL_CONDITION_SATISFIED) {
            _prefetchFenceState.store(
                _FenceState::kReady, std::memory_order_release);
            return true;
        }
        if (result == GL_WAIT_FAILED) {
            break;
        }
    }
    TF_WARN(
        "GPU buffer readback prefetch did not complete; reading back on "
        "demand this frame");
    _prefetchFenceState.store(_FenceState::kFailed, std::memory_order_release);
    return false;
}

std::shared_ptr<_Entry> _FindOrInsertEntry(const ReadKey& key)
{
    std::lock_guard<std::mutex> lock(_cacheMutex);
    std::shared_ptr<_Entry>& cached = _cache[key];
    if (!cached) {
        cached = std::make_shared<_Entry>();
    }
    return cached;
}

} // namespace

void Prefetch(const ReadKey& key, size_t spanBytes)
{
    // Without frames nothing resets the staging, and the cache is bypassed.
    if (!_PrefetchEnabled() || spanBytes == 0 || key.rawHandle == 0
        || !_frameBegan.load(std::memory_order_acquire)
        || !glCopyNamedBufferSubData) {
        return;
    }
    const std::shared_ptr<_ReadbackContext> readback = _GetReadbackContext();
    // The copy belongs in Maya's command stream, after VP2's writes and
    // before the fence EndFrame puts there.
    if (!readback || _GetCurrentContext() != readback->mayaContext) {
        return;
    }

    const std::shared_ptr<_Entry> entry = _FindOrInsertEntry(key);
    std::lock_guard<std::mutex> lock(entry->mutex);
    if (entry->done || entry->staged) {
        // Already pulled this frame, or already queued by another item
        // drawing from the same buffer.
        return;
    }

    GLuint buffer = 0;
    size_t offset = 0;
    const unsigned char* const staged =
        _AllocateStaging(*readback, spanBytes, &buffer, &offset);
    if (!staged) {
        return;
    }
    // Normally no glGetError here: on a threaded driver it synchronizes with
    // the driver thread, once per stream, and the span is the one the adapter
    // published, the same layout the copying bridge copies with. A copy the
    // driver rejects would then go unnoticed and the pull would decode stale
    // staging, so the debug code checks each copy and drops the entry back to
    // the on-demand readback on failure. That check drains Maya's error flag
    // first, which is acceptable only while debugging.
    const bool checkCopy =
        TfDebug::IsEnabled(MAYAHYDRALIB_ADAPTER_GPU_BUFFER_SHARING);
    if (checkCopy) {
        for (int i = 0; i < 32 && glGetError() != GL_NO_ERROR; ++i) {
        }
    }
    glCopyNamedBufferSubData(
        static_cast<GLuint>(key.rawHandle),
        buffer,
        static_cast<GLintptr>(key.byteOffset),
        static_cast<GLintptr>(offset),
        static_cast<GLsizeiptr>(spanBytes));
    if (checkCopy) {
        const GLenum error = glGetError();
        if (error != GL_NO_ERROR) {
            TF_DEBUG_EXT_GPU_BUFFER_READBACK(
                "prefetch copy of GL buffer %llu (offset %zu, %zu bytes) "
                "failed with OpenGL error 0x%x; reading back on demand\n",
                static_cast<unsigned long long>(key.rawHandle),
                key.byteOffset, spanBytes, static_cast<unsigned int>(error));
            return;
        }
    }
    entry->staged = staged;
    entry->stagedBytes = spanBytes;
    entry->stagedContext = readback.get();
    _prefetchCopies.fetch_add(1, std::memory_order_relaxed);
}

VtValue GetOrRead(
    const ReadKey&                                                             key,
    const std::function<VtValue(const unsigned char* span, size_t spanBytes)>& decode,
    const std::function<VtValue()>&                                            fetch)
{
    MH_PROFILE_FUNCTION();

    // Checked first so steady-state pulls do not all write the same cache line.
    if (!_cpuConsumerSeen.load(std::memory_order_relaxed)
        && !_cpuConsumerSeen.exchange(true, std::memory_order_acq_rel)) {
        TF_DEBUG_EXT_GPU_BUFFER_READBACK(
            "first CPU pull of a shared buffer: frame fencing starts from the "
            "next producer frame\n");
    }

    if (!_frameBegan.load(std::memory_order_acquire)) {
        _fetchedReads.fetch_add(1, std::memory_order_relaxed);
        return fetch();
    }

    const std::shared_ptr<_Entry> entry = _FindOrInsertEntry(key);

    // Per buffer: the first reader reads, readers of the same buffer wait for
    // it, readers of other buffers do not.
    std::lock_guard<std::mutex> lock(entry->mutex);
    if (entry->done) {
        _cacheHits.fetch_add(1, std::memory_order_relaxed);
        return entry->value;
    }
    // A failed read is cached too: it fails the same way for the rest of the
    // frame, so retrying it would only repeat the stall.
    if (entry->staged && entry->stagedContext == _GetReadbackContext().get()
        && _WaitPrefetchFence()) {
        _prefetchedReads.fetch_add(1, std::memory_order_relaxed);
        entry->value = decode(entry->staged, entry->stagedBytes);
    } else {
        _fetchedReads.fetch_add(1, std::memory_order_relaxed);
        entry->value = fetch();
    }
    entry->done = true;
    return entry->value;
}

Lease::Lease()
{
    _readback = _GetReadbackContext();
    if (!_readback) {
        static std::once_flag warned;
        std::call_once(warned, []() {
            TF_WARN(
                "Cannot read a shared VP2 vertex buffer: no GL context "
                "sharing Maya's resources has been created");
        });
        return;
    }

    _lock = std::unique_lock<std::mutex>(_readbackMutex);
    _scope.emplace(*_readback->gl);
    if (!*_scope) {
        return;
    }

    // Orders the read after everything VP2 wrote this frame on Maya's
    // context. Absent only for a pull before the producer's frame closed.
    if (const GLsync fence = _readback->frameFence.load()) {
        if (glWaitSync) {
            glWaitSync(fence, 0, GL_TIMEOUT_IGNORED);
        }
    }
}

} // namespace MhExtGpuBufferReadback

PXR_NAMESPACE_CLOSE_SCOPE
