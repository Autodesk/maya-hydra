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

#ifndef LIB_MAYAHYDRA_HYDRAEXTENSIONS_ADAPTERS_MHEXTGPUBUFFERREADBACK_H
#define LIB_MAYAHYDRA_HYDRAEXTENSIONS_ADAPTERS_MHEXTGPUBUFFERREADBACK_H

#include <mayaHydraLib/mhSharedGLContext.h>

#include <pxr/base/vt/value.h>
#include <pxr/pxr.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <typeindex>
#include <typeinfo>

PXR_NAMESPACE_OPEN_SCOPE

/// Reads VP2's GL buffers back to the CPU from Hydra worker threads.
///
/// Readback goes through one hidden context in Maya's share group, with its
/// own drawable, made current under a global lock: one thread reads back at a
/// time. More contexts were measured to buy nothing -- the driver serializes
/// the round trips anyway -- and the prefetch below removes most of them.
///
/// The context is built once per renderer, on the first publish, and dropped
/// in Shutdown(). Maya renders every VP2 panel with one GL context and never
/// replaces it while running, so there is nothing to rebuild for.
///
/// Reads are also deduplicated per frame: two render items drawing from the
/// same VP2 buffer (shaded + wireframe) read it back once.
///
/// Streams a CPU consumer is known to read are prefetched: while publishing,
/// the main thread queues one GPU copy per stream into persistently mapped
/// staging memory, all covered by the producer frame's fence. A pull then
/// waits on that fence once per frame and decodes from mapped memory with no
/// GL call, instead of paying a synchronous GPU round trip per stream.
namespace MhExtGpuBufferReadback {

/// Build the readback context, sharing with Maya's current GL context, unless
/// this renderer already tried. Main thread only, with Maya's context current.
/// After the first attempt it costs one atomic load; with no context current
/// it does nothing and the next call tries again. A failed attempt is not
/// retried before Shutdown().
void Capture();

/// Open the producer's frame: drop last frame's cached readbacks and fence,
/// and destroy retired readback contexts no Lease still holds. Main thread, Maya's
/// context current, no readback in flight.
void BeginFrame();

/// Close the producer's frame: fence Maya's context so that readbacks and
/// prefetch reads are ordered after everything VP2 wrote this frame. Main
/// thread, Maya's context current. Does nothing until some consumer has pulled
/// a CPU value, so a session whose consumers all take extGpuBuffer pays no
/// per-frame fence or flush.
void EndFrame();

/// Release the readback context, its drawable and the prefetch staging.
/// Main thread, Maya's context current, before the renderer is torn down.
void Shutdown();

/// Identifies one stream's readback. Not just the GL name: an interleaved VP2
/// buffer holds several streams under one name.
struct ReadKey
{
    uint64_t        rawHandle = 0;
    size_t          byteOffset = 0;
    size_t          byteStride = 0;
    size_t          numElements = 0;
    std::type_index elementType = typeid(void);

    bool operator==(const ReadKey& other) const
    {
        return rawHandle == other.rawHandle
            && byteOffset == other.byteOffset
            && byteStride == other.byteStride
            && numElements == other.numElements
            && elementType == other.elementType;
    }
};

/// Queue a GPU copy of \p spanBytes bytes of the stream, starting at
/// key.byteOffset, into staging, so this frame's pull of \p key is served
/// from it. Main thread, Maya's context current, inside the producer frame.
/// A no-op whenever that cannot be done; the pull then reads back directly.
void Prefetch(const ReadKey& key, size_t spanBytes);

/// Return this frame's value for \p key. The first caller produces it: by
/// \p decode, from the staged span and its byte size when \p key was
/// prefetched this frame, otherwise by \p fetch. Concurrent callers for the
/// same key wait for the first; callers for different keys do not wait for
/// each other.
VtValue GetOrRead(
    const ReadKey&                                                             key,
    const std::function<VtValue(const unsigned char* span, size_t spanBytes)>& decode,
    const std::function<VtValue()>&                                            fetch);

struct _ReadbackContext;

/// Takes the global readback lock and makes the readback context current on
/// this thread for the scope, restoring whatever was current before. Converts
/// to false when the context could not be made current; the caller must then
/// not issue GL.
class Lease
{
public:
    Lease();

    Lease(const Lease&) = delete;
    Lease& operator=(const Lease&) = delete;

    explicit operator bool() const { return _scope && *_scope; }

private:
    // Keeps the context alive for the scope, even if Capture() replaces it.
    std::shared_ptr<_ReadbackContext> _readback;
    std::unique_lock<std::mutex>      _lock;

    // Declared last so it is destroyed first: the context is released before
    // the lock is.
    std::optional<MAYAHYDRA_NS::MhSharedGLContext::Scope> _scope;
};

} // namespace MhExtGpuBufferReadback

PXR_NAMESPACE_CLOSE_SCOPE

#endif // LIB_MAYAHYDRA_HYDRAEXTENSIONS_ADAPTERS_MHEXTGPUBUFFERREADBACK_H
