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

#include "mhExtGpuBufferBridge.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/garch/glApi.h>
#include <pxr/imaging/hgi/enums.h>
#include <pxr/imaging/hgi/externalBufferArena.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/semaphore.h>
#include <pxr/imaging/hgi/tokens.h>
#include <pxr/imaging/hgiGL/externalBufferArena.h>
#include <pxr/imaging/hgiGL/semaphore.h>

#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
#include <pxr/imaging/hgiVulkan/externalBuffer.h>
#include <pxr/imaging/hgiVulkan/externalBufferArena.h>
#endif

#include <maya/MViewport2Renderer.h>

#include <algorithm>
#include <mutex>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

namespace {

constexpr HgiBufferUsage kExtBufferUsage =
    HgiBufferUsageVertex | HgiBufferUsageStorage;

// Whether VP2 is drawing through OpenGL, which is the only API whose buffers
// we can reach. MVertexBuffer::resourceHandle() is an opaque void* whatever
// the backend, so a Metal buffer pointer read as a GL name would bind some
// unrelated object; the question has to be asked of the renderer rather than
// inferred from the handle.
bool
_ProducerIsOpenGL()
{
    auto *renderer = MHWRender::MRenderer::theRenderer();
    if (!renderer) {
        return false;
    }
    const unsigned int api = renderer->drawAPI();
    return api == MHWRender::kOpenGL || api == MHWRender::kOpenGLCoreProfile;
}

#if defined(MAYAHYDRA_HAS_HGI_VULKAN)

// Clear the GL error queue so that a later glGetError reports on our own call
// and not on whatever Maya left behind. Without this a pending error from
// elsewhere fails a perfectly good copy, and the stream drops to a CPU path
// that cannot read a VP2 buffer -- which shows up as geometry vanishing.
void
_DrainGlErrors()
{
    while (glGetError() != GL_NO_ERROR) {
    }
}

// The GL objects aliasing one Hgi-owned allocation: the imported memory, and
// the buffer bound over it that Maya's data is copied into. The memory object
// is shared with every other alias out of the same Vulkan memory block; see
// _AcquireGlMemoryObject.
struct _GlAlias
{
    GLuint memoryObject = 0;
    GLuint buffer = 0;
};

// One GL memory object per Vulkan memory BLOCK, not per buffer.
//
// VMA suballocates, and the export handle names the whole block: a dozen
// external buffers routinely live inside one 32 MiB block at different
// offsets. Importing per buffer therefore imports the entire block once per
// buffer -- 43 buffers holding 6 MB of data asked GL to hold 43 x 32 MiB, and
// glNamedBufferStorageMemEXT began failing with GL_OUT_OF_MEMORY. The streams
// that failed fell back to a CPU path that cannot read a VP2 buffer, which is
// what took the geometry with it.
//
// The export info carries no block identity to key on, so the handles are
// compared instead. GetWin32HandleForMemory caches one handle per
// VkDeviceMemory and returns a fresh duplicate per call, so two buffers from
// one block yield two handles naming the same kernel object -- precisely the
// question CompareObjectHandles answers.
struct _GlMemoryBlock
{
    void  *handle = nullptr; // retained duplicate naming the block, or null
    GLuint memoryObject = 0;
    int    refCount = 0;
};
std::mutex                  _glBlocksMutex;
std::vector<_GlMemoryBlock> _glBlocks;

#if defined(ARCH_OS_WINDOWS)
// Whether two handles name the same kernel object.
//
// CompareObjectHandles lives in kernelbase and is guarded by a newer
// _WIN32_WINNT than this build targets, so it is resolved at runtime rather
// than linked. Absent it, no two blocks ever match and every buffer imports
// its own -- correct, but back to the duplication this exists to avoid.
bool
_HandlesNameSameObject(void *a, void *b)
{
    using CompareFn = BOOL(WINAPI *)(HANDLE, HANDLE);
    static const CompareFn compare = []() -> CompareFn {
        if (HMODULE kernelBase = GetModuleHandleW(L"kernelbase.dll")) {
            return reinterpret_cast<CompareFn>(
                GetProcAddress(kernelBase, "CompareObjectHandles"));
        }
        return nullptr;
    }();
    return compare
        && compare(static_cast<HANDLE>(a), static_cast<HANDLE>(b)) != FALSE;
}
#endif

// Destroying GL objects needs a current context, and the last reference to an
// external buffer is dropped by HgiExternalBufferArena::GarbageCollect() on
// whichever thread reaches it -- which the API documents as "enqueue rather
// than call GPU APIs inline". So the keepalive deleter only queues, and the
// queue is drained from the publish path below, which runs on Maya's thread
// with Maya's context current.
std::mutex              _pendingGlDeletesMutex;
std::vector<_GlAlias>   _pendingGlDeletes;

// Every alias buffer currently sharing memory with the consumer.
//
// The semaphore bracket needs this as a list: glSignalSemaphoreEXT and
// glWaitSemaphoreEXT establish coherency only for the objects named in their
// barrier list, so a shared buffer left out of it is not made visible however
// correctly the semaphore itself is paired.
std::mutex              _liveAliasesMutex;
std::vector<GLuint>     _liveAliasBuffers;

void
_RegisterLiveAlias(const _GlAlias &alias)
{
    std::lock_guard<std::mutex> lock(_liveAliasesMutex);
    _liveAliasBuffers.push_back(alias.buffer);
}

std::vector<GLuint>
_GetLiveAliases()
{
    std::lock_guard<std::mutex> lock(_liveAliasesMutex);
    return _liveAliasBuffers;
}

void
_QueueGlDelete(const _GlAlias &alias)
{
    // Out of the barrier list the moment it stops being shared, rather than
    // when the GL objects are finally destroyed: naming a dead buffer in a
    // barrier list is an error, and the delete queue is drained later.
    {
        std::lock_guard<std::mutex> lock(_liveAliasesMutex);
        _liveAliasBuffers.erase(
            std::remove(_liveAliasBuffers.begin(), _liveAliasBuffers.end(),
                        alias.buffer),
            _liveAliasBuffers.end());
    }
    std::lock_guard<std::mutex> lock(_pendingGlDeletesMutex);
    _pendingGlDeletes.push_back(alias);
}

// Drop one reference to a shared memory object, destroying it with the last.
// Called only from the drain below, so a current context is guaranteed.
void
_ReleaseGlMemoryObject(GLuint memoryObject)
{
    void *handleToClose = nullptr;
    bool  destroy = false;
    {
        std::lock_guard<std::mutex> lock(_glBlocksMutex);
        for (auto it = _glBlocks.begin(); it != _glBlocks.end(); ++it) {
            if (it->memoryObject != memoryObject) {
                continue;
            }
            if (--it->refCount <= 0) {
                handleToClose = it->handle;
                destroy = true;
                _glBlocks.erase(it);
            }
            break;
        }
    }
    if (destroy && glDeleteMemoryObjectsEXT) {
        glDeleteMemoryObjectsEXT(1, &memoryObject);
    }
#if defined(ARCH_OS_WINDOWS)
    if (handleToClose) {
        CloseHandle(static_cast<HANDLE>(handleToClose));
    }
#endif
}

void
_DrainGlDeletes()
{
    std::vector<_GlAlias> doomed;
    {
        std::lock_guard<std::mutex> lock(_pendingGlDeletesMutex);
        doomed.swap(_pendingGlDeletes);
    }
    for (const _GlAlias &alias : doomed) {
        if (alias.buffer) {
            glDeleteBuffers(1, &alias.buffer);
        }
        if (alias.memoryObject) {
            _ReleaseGlMemoryObject(alias.memoryObject);
        }
    }
}

// The GL memory object aliasing the block that \p info names, importing it on
// first use and sharing it thereafter. Returns 0 on failure.
//
// Consumes \p info.externalHandle either way: the handle is our duplicate, so
// it is closed once matched against an existing block, and retained by the
// block it creates so later buffers can be recognised.
GLuint
_AcquireGlMemoryObject(
    const HgiVulkanExternalBufferExportInfo &info,
    size_t                                   byteSize)
{
#if defined(ARCH_OS_WINDOWS)
    HANDLE incoming =
        reinterpret_cast<HANDLE>(static_cast<uintptr_t>(info.externalHandle));
    if (info.handleType != HgiExternalHandleTypeOpaqueWin32
            || !glImportMemoryWin32HandleEXT) {
        return 0;
    }
    {
        std::lock_guard<std::mutex> lock(_glBlocksMutex);
        for (_GlMemoryBlock &block : _glBlocks) {
            if (block.handle
                    && _HandlesNameSameObject(block.handle, incoming)) {
                ++block.refCount;
                // Already have this block imported; our duplicate is surplus.
                CloseHandle(incoming);
                return block.memoryObject;
            }
        }
    }
#else
    // No portable way to tell whether two fds name the same allocation, so
    // every buffer imports its own block here. Correct, but it pays the
    // duplication this sharing exists to avoid.
    if (info.handleType != HgiExternalHandleTypeOpaqueFd
            || !glImportMemoryFdEXT) {
        return 0;
    }
#endif

    GLuint memoryObject = 0;
    glCreateMemoryObjectsEXT(1, &memoryObject);
    if (!memoryObject) {
#if defined(ARCH_OS_WINDOWS)
        CloseHandle(incoming);
#endif
        return 0;
    }

    // A dedicated allocation has to be imported as dedicated; the parameter is
    // not advisory, and mismatching it fails the import.
    if (info.dedicated && glMemoryObjectParameterivEXT) {
        const GLint dedicated = GL_TRUE;
        glMemoryObjectParameterivEXT(
            memoryObject, GL_DEDICATED_MEMORY_OBJECT_EXT, &dedicated);
    }

    // The whole block, which is what the handle names and what the size has
    // to match; individual buffers are bound at their offsets into it.
#if defined(ARCH_OS_WINDOWS)
    glImportMemoryWin32HandleEXT(
        memoryObject, info.memoryBlockSize, GL_HANDLE_TYPE_OPAQUE_WIN32_EXT,
        incoming);
#else
    // The import takes over the fd.
    glImportMemoryFdEXT(
        memoryObject, info.memoryBlockSize, GL_HANDLE_TYPE_OPAQUE_FD_EXT,
        static_cast<int>(info.externalHandle));
#endif

    const GLenum importErr = glGetError();
    if (importErr != GL_NO_ERROR) {
        glDeleteMemoryObjectsEXT(1, &memoryObject);
#if defined(ARCH_OS_WINDOWS)
        CloseHandle(incoming);
#endif
        return 0;
    }

    {
        std::lock_guard<std::mutex> lock(_glBlocksMutex);
        _GlMemoryBlock block;
        // Win32 import duplicates rather than consumes, so the handle stays
        // ours -- and is kept, because it is the only thing that identifies
        // this block when the next buffer out of it arrives.
#if defined(ARCH_OS_WINDOWS)
        block.handle = incoming;
#endif
        block.memoryObject = memoryObject;
        block.refCount = 1;
        _glBlocks.push_back(block);
    }
    return memoryObject;
}

// Bind a GL buffer over the memory Hgi allocated, so Maya can write into
// something the Vulkan consumer is simultaneously reading. Mirrors the import
// in hgiGL's own external buffer, which is the same operation from the other
// side of the same extension.
bool
_ImportIntoGl(
    const HgiVulkanExternalBufferExportInfo &info,
    size_t                                   byteSize,
    _GlAlias                                *outAlias)
{
    if (!info.externalHandle || info.memoryBlockSize == 0) {
        return false;
    }
    if (!glCreateMemoryObjectsEXT || !glNamedBufferStorageMemEXT) {
        return false;
    }

    _DrainGlErrors();

    const GLuint memoryObject = _AcquireGlMemoryObject(info, byteSize);
    if (!memoryObject) {
        return false;
    }

    _GlAlias alias;
    alias.memoryObject = memoryObject;
    glCreateBuffers(1, &alias.buffer);
    if (!alias.buffer) {
        _ReleaseGlMemoryObject(memoryObject);
        return false;
    }

    // Bound at this buffer's offset within the shared block, which is what
    // makes one imported memory object serve every suballocation in it.
    glNamedBufferStorageMemEXT(
        alias.buffer, byteSize, memoryObject, info.memoryOffset);

    // A failed storage call leaves a buffer with no memory behind it, which
    // would otherwise be handed out as if the import had worked.
    const GLenum storageErr = glGetError();
    if (storageErr != GL_NO_ERROR) {
        glDeleteBuffers(1, &alias.buffer);
        _ReleaseGlMemoryObject(memoryObject);
        return false;
    }

    *outAlias = alias;
    return true;
}

bool
_CopyIntoAlias(GLuint mayaGlBuffer, const _GlAlias &alias, size_t copyByteSize)
{
    if (!glCopyNamedBufferSubData || !alias.buffer || copyByteSize == 0) {
        return false;
    }
    _DrainGlErrors();
    glCopyNamedBufferSubData(
        mayaGlBuffer, alias.buffer, 0, 0,
        static_cast<GLsizeiptr>(copyByteSize));
    const GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        static std::once_flag warned;
        std::call_once(warned, [err, copyByteSize]() {
            TF_WARN("[mayaHydra] Copying a VP2 vertex buffer into shared "
                    "memory failed with GL error 0x%x (%zu bytes); GPU buffer "
                    "sharing is falling back to the CPU path.",
                    unsigned(err), copyByteSize);
        });
        return false;
    }
    return true;
}

const _GlAlias *
_AliasOf(const HgiExternalBufferSharedPtr &buffer)
{
    return static_cast<const _GlAlias *>(buffer->GetKeepalive().get());
}

// The producer's half of the arena's synchronisation bracket.
//
// Touched only from the publish pass, which runs on Maya's thread with Maya's
// GL context current -- the same constraint the copies themselves have, and the
// reason no lock guards this.
struct _Sync
{
    HgiGLImportedSemaphoreSharedPtr appDone;
    HgiGLImportedSemaphoreSharedPtr hgiDone;
    HgiExternalBufferArena         *arena = nullptr;

    // Whether a signal is outstanding: app-done was signalled for a publish
    // that the consumer has not reported finishing with yet. A binary
    // semaphore carries no count, so waiting without this would block on a
    // signal that is not coming.
    bool awaitingHgiDone = false;

    // Set once creation has been attempted, successfully or not, so a failure
    // is not retried on every allocation.
    bool attempted = false;
};
_Sync _sync;

bool
_HaveSemaphores()
{
    return _sync.appDone && _sync.hgiDone;
}

// Create the arena's exportable pair and import both into GL.
//
// Sharing is all-or-nothing on this: a copying producer that cannot tell the
// consumer when its writes are finished, or be told when the reads are, has
// no safe way to hand a buffer over, so a failure here declines sharing
// altogether rather than proceeding on a weaker guarantee.
bool
_EnsureSync(HgiVulkanExternalBufferArena *arena)
{
    if (_sync.attempted) {
        return _HaveSemaphores();
    }
    _sync.attempted = true;

    uint64_t appDoneHandle = 0;
    uint64_t hgiDoneHandle = 0;
    // Binary, because that is all either side can do: GL_EXT_semaphore has no
    // timeline form, and the Vulkan command queue's pending wait and signal
    // lists carry no values.
    if (!arena->CreateExportableSemaphores(
            HgiSemaphoreKindBinary, &appDoneHandle, &hgiDoneHandle)) {
        return false;
    }

#if defined(ARCH_OS_WINDOWS)
    constexpr HgiExternalHandleType kHandleType =
        HgiExternalHandleTypeOpaqueWin32;
#else
    constexpr HgiExternalHandleType kHandleType =
        HgiExternalHandleTypeOpaqueFd;
#endif

    HgiGLImportedSemaphoreSharedPtr appDone = HgiGLImportedSemaphore::Import(
        appDoneHandle, kHandleType, HgiSemaphoreKindBinary);
    HgiGLImportedSemaphoreSharedPtr hgiDone = HgiGLImportedSemaphore::Import(
        hgiDoneHandle, kHandleType, HgiSemaphoreKindBinary);

#if defined(ARCH_OS_WINDOWS)
    // Win32 import duplicates rather than consumes, and Hgi keeps the value it
    // exported without ever closing it, so these copies are ours to close.
    for (const uint64_t handle : { appDoneHandle, hgiDoneHandle }) {
        if (handle) {
            CloseHandle(
                reinterpret_cast<HANDLE>(static_cast<uintptr_t>(handle)));
        }
    }
#endif

    if (!appDone || !hgiDone) {
        // The arena is already holding the pair it just created, and Hgi would
        // then wait on an app-done semaphore nothing can signal -- a hang, not
        // a slow path. ImportSemaphores with null handles installs none, which
        // puts the arena back to unsynchronised before sharing is declined.
        arena->ImportSemaphores(0, 0, kHandleType, HgiSemaphoreKindBinary);
        TF_WARN("[mayaHydra] Could not import the renderer's external buffer "
                "semaphores into GL; GPU buffer sharing is disabled.");
        return false;
    }

    _sync.appDone = std::move(appDone);
    _sync.hgiDone = std::move(hgiDone);
    _sync.arena = arena;
    return true;
}

#endif // MAYAHYDRA_HAS_HGI_VULKAN

} // namespace

MhExtGpuBufferBridge
MhExtGpuBufferBridge::ForHgi(Hgi *hgi)
{
    MhExtGpuBufferBridge bridge;
    if (!hgi || !_ProducerIsOpenGL()) {
        return bridge;
    }

    // Get-or-create, so every call after the first is a lookup, and null means
    // this backend cannot consume external buffers at all.
    if (hgi->GetAPIName() == HgiTokens->OpenGL) {
        if (auto arena = hgi->GetExternalBufferArena<HgiGLExternalBufferArena>()) {
            bridge._kind = _Kind::OpenGL;
            bridge._arena = arena.get();
        }
        return bridge;
    }

#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    if (hgi->GetAPIName() == HgiTokens->Vulkan) {
        if (auto arena =
                hgi->GetExternalBufferArena<HgiVulkanExternalBufferArena>()) {
            bridge._kind = _Kind::Vulkan;
            bridge._arena = arena.get();
        }
    }
#endif
    return bridge;
}

const char *
MhExtGpuBufferBridge::Describe() const
{
    switch (_kind) {
    case _Kind::OpenGL: return "OpenGL";
    case _Kind::Vulkan: return "Vulkan";
    case _Kind::None:   break;
    }
    return "none";
}

HgiExternalBufferSharedPtr
MhExtGpuBufferBridge::Create(
    uint32_t    mayaGlBuffer,
    size_t      byteSize,
    size_t      copyByteSize,
    const char *debugName) const
{
    if (_kind == _Kind::OpenGL) {
        // REGISTER, never adopt. VP2 owns these buffers and recycles them on
        // its own schedule, so the arena binds and reads them and must never
        // delete one: that would be a double free, and GL may hand the freed
        // name straight back out for an unrelated allocation.
        //
        // Registration is the weaker of the two contracts -- the producer
        // promises the buffer outlives the renderer's use of it and nothing
        // enforces that promise. What makes it sound here is that VP2 and
        // Storm share a single GL context: commands on one context execute in
        // issue order, so a recycle VP2 issues after the draw that read the
        // buffer is ordered after that read, and glDeleteBuffers is deferred
        // by the driver until the GPU is finished.
        return static_cast<HgiGLExternalBufferArena *>(_arena)
            ->RegisterBuffer(mayaGlBuffer, byteSize, kExtBufferUsage);
    }

#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    if (_kind == _Kind::Vulkan) {
        _DrainGlDeletes();

        // Hgi owns the memory, because Maya's cannot be shared: GL never
        // exported an allocation in its life, and the binding between a GL
        // buffer and its storage is fixed when the storage is created. So the
        // shared allocation has to come from the side that can export.
        auto *arena = static_cast<HgiVulkanExternalBufferArena *>(_arena);

        // Before the first allocation, so the pair is in place for the frame
        // that publishes it: the arena only waits for a publish it was told
        // about, and only signals for a publish it waited for.
        if (!_EnsureSync(arena)) {
            return nullptr;
        }

        HgiExternalBufferSharedPtr buffer =
            arena->AllocateBuffer(byteSize, kExtBufferUsage, debugName);
        if (!buffer) {
            // Loud, and once: the caller's response is to fall back to the CPU
            // primvar, which for a VP2 buffer Maya will not map is no geometry
            // at all. Silence here reads as objects dropping out of the
            // viewport with nothing to connect them to.
            static std::once_flag warned;
            std::call_once(warned, [arena]() {
                const HgiExternalBufferArenaUsage usage = arena->GetUsage();
                TF_WARN("[mayaHydra] The Vulkan external buffer arena could "
                        "not allocate; GPU buffer sharing is falling back to "
                        "the CPU path. Arena holds %zu buffers (%zu bytes), "
                        "%zu awaiting destruction.",
                        usage.numBuffers, usage.totalByteSize,
                        usage.numPendingDestroy);
            });
            return nullptr;
        }

        const HgiVulkanExternalBufferExportInfo &info =
            static_cast<HgiVulkanExternalBuffer *>(buffer.get())
                ->GetExportInfo();

        _GlAlias alias;
        if (!_ImportIntoGl(info, byteSize, &alias)) {
            // Dropping the only reference hands the allocation back to the
            // arena, which frees it on its next sweep.
            return nullptr;
        }

        // The GL objects live exactly as long as the buffer that owns the
        // memory they alias, which the keepalive expresses directly and no
        // side table would.
        buffer->SetKeepalive(std::shared_ptr<_GlAlias>(
            new _GlAlias(alias),
            [](_GlAlias *a) {
                _QueueGlDelete(*a);
                delete a;
            }));
        _RegisterLiveAlias(alias);

        if (!_CopyIntoAlias(mayaGlBuffer, alias, copyByteSize)) {
            return nullptr;
        }
        return buffer;
    }
#endif

    (void)mayaGlBuffer;
    (void)byteSize;
    (void)copyByteSize;
    (void)debugName;
    return nullptr;
}

bool
MhExtGpuBufferBridge::Refresh(
    const HgiExternalBufferSharedPtr &buffer,
    uint32_t                          mayaGlBuffer,
    size_t                            copyByteSize) const
{
    if (!buffer) {
        return false;
    }
    if (_kind == _Kind::OpenGL) {
        // The consumer is bound to Maya's own buffer, so a rewrite in place is
        // already visible to it and there is nothing to bring in step.
        return true;
    }

#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    if (_kind == _Kind::Vulkan) {
        const _GlAlias *alias = _AliasOf(buffer);
        if (!alias) {
            return false;
        }
        return _CopyIntoAlias(mayaGlBuffer, *alias, copyByteSize);
    }
#endif

    (void)mayaGlBuffer;
    (void)copyByteSize;
    return false;
}

void
MhExtGpuBufferBridge::BeginProducerFrame()
{
#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    // Nothing published is outstanding, so nothing has been handed to the
    // consumer that it could still be reading. This also covers the first
    // frame, when the semaphores do not exist yet.
    if (!_sync.awaitingHgiDone || !_HaveSemaphores()) {
        return;
    }
    _sync.awaitingHgiDone = false;

    // The copies below this point overwrite buffers the consumer's last frame
    // drew from, so they have to wait for those reads to retire.
    const std::vector<GLuint> aliases = _GetLiveAliases();
    glWaitSemaphoreEXT(
        _sync.hgiDone->GetSemaphoreId(),
        static_cast<GLuint>(aliases.size()),
        aliases.empty() ? nullptr : aliases.data(),
        0, nullptr, nullptr);
#endif
}

void
MhExtGpuBufferBridge::EndProducerFrame()
{
#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    // The one point per frame that reliably holds Maya's context, which is
    // what destroying the GL objects the arena has released needs.
    _DrainGlDeletes();

    if (!_HaveSemaphores()) {
        return;
    }

    // Signalled every frame, whether or not anything was copied. The signal
    // and the wait are what move the shared buffers between Vulkan's queue
    // family and ours -- the consumer releases them to VK_QUEUE_FAMILY_EXTERNAL
    // on its hgi-done signal and takes them back on its app-done wait -- so
    // skipping a frame leaves the buffers released, and a consumer reading a
    // buffer its queue family does not own sees undefined contents.
    //
    // Publish, then tell the arena, and both before the consumer's frame
    // starts. The arena encodes its wait in StartFrame and only for an epoch
    // it has been told about, so a NotifyAppDone landing after StartFrame is
    // not picked up until the next frame, and the wait above would then block
    // a frame longer than it should.
    const std::vector<GLuint> aliases = _GetLiveAliases();
    glSignalSemaphoreEXT(
        _sync.appDone->GetSemaphoreId(),
        static_cast<GLuint>(aliases.size()),
        aliases.empty() ? nullptr : aliases.data(),
        0, nullptr, nullptr);

    _sync.arena->NotifyAppDone();
    _sync.awaitingHgiDone = true;
#endif
}

void
MhExtGpuBufferBridge::Shutdown()
{
#if defined(MAYAHYDRA_HAS_HGI_VULKAN)
    // Everything GL-side has to go while a context is still current and Tf's
    // diagnostic delegates are still alive. Left to static destruction the
    // imported semaphores outlive both: HgiGLImportedSemaphore's destructor
    // calls glDeleteSemaphoresEXT with no context, and the GL error that
    // follows is posted through a delegate list that has already been torn
    // down -- a crash on shutdown rather than a leak.
    _sync = _Sync {};

    _DrainGlDeletes();

    std::vector<_GlMemoryBlock> blocks;
    {
        std::lock_guard<std::mutex> lock(_glBlocksMutex);
        blocks.swap(_glBlocks);
    }
    for (const _GlMemoryBlock &block : blocks) {
        if (block.memoryObject && glDeleteMemoryObjectsEXT) {
            GLuint memoryObject = block.memoryObject;
            glDeleteMemoryObjectsEXT(1, &memoryObject);
        }
#if defined(ARCH_OS_WINDOWS)
        if (block.handle) {
            CloseHandle(static_cast<HANDLE>(block.handle));
        }
#endif
    }

    {
        std::lock_guard<std::mutex> lock(_liveAliasesMutex);
        _liveAliasBuffers.clear();
    }
#endif
}

PXR_NAMESPACE_CLOSE_SCOPE
