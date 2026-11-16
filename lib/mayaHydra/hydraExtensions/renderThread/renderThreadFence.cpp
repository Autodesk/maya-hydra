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

#include "renderThreadFence.h"

// GL loading library needs to be included before any other OpenGL headers.
#include <pxr/imaging/garch/glApi.h>

#include <pxr/base/tf/diagnostic.h>
#include <pxr/imaging/hgi/hgi.h>
#include <pxr/imaging/hgi/tokens.h>

#include <chrono>
#include <thread>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF
{

RenderThreadFencePtr
RenderThreadFence::CreateAndSignal(Hgi* hgi)
{
    // A GL sync object is neither needed nor possible under Vulkan: no GL
    // context is current on the Vulkan render thread, and HgiInteropVulkan's
    // semaphore protocol already provides the ordering.
    if (hgi && hgi->GetAPIName() == HgiTokens->Vulkan)
    {
        return RenderThreadFencePtr(new RenderThreadFence(nullptr, /*isNoop=*/true));
    }

    GLsync created = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);

    if (created)
    {
        // Without this the fence may never make progress, because the commands
        // ahead of it have not been submitted.
        glFlush();
    }
    else
    {
        TF_WARN("RenderThreadFence: glFenceSync returned 0. No GL context is "
                "current on the calling thread, so GPU completion cannot be "
                "observed.");
    }

    // Not make_shared: the constructor is private.
    return RenderThreadFencePtr(new RenderThreadFence(created, /*isNoop=*/false));
}

RenderThreadFence::~RenderThreadFence()
{
    if (!_glFence)
    {
        return;
    }

    // No waiter can be inside glClientWaitSync on this object: a waiter holds a
    // shared_ptr for the duration of its wait, so reaching the destructor means
    // there is none. That is why this class needs no mutex.
    glDeleteSync(reinterpret_cast<GLsync>(_glFence));
}

bool
RenderThreadFence::WaitOnGpu() const
{
    // Vulkan: HgiInteropVulkan's semaphores already establish the ordering.
    if (_isNoop)
    {
        return true;
    }

    if (!_glFence)
    {
        return false;
    }

    // The spec requires flags to be 0 and the timeout to be GL_TIMEOUT_IGNORED;
    // anything else is an error rather than a shorter wait.
    glWaitSync(reinterpret_cast<GLsync>(_glFence), 0, GL_TIMEOUT_IGNORED);
    return true;
}

bool
RenderThreadFence::Wait(uint64_t timeoutNanos) const
{
    // Vulkan: HgiInteropVulkan's semaphores already establish the ordering.
    if (_isNoop)
    {
        return true;
    }

    // A fence that was never created cannot say the AOV is ready.
    if (!_glFence)
    {
        return false;
    }

    GLsync target = reinterpret_cast<GLsync>(_glFence);

    // A single non-blocking poll, so Wait(0) does not pay for the loop below.
    if (timeoutNanos == 0)
    {
        const GLenum result = glClientWaitSync(target, 0, 0);
        return result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED;
    }

    // Flags stay 0 rather than GL_SYNC_FLUSH_COMMANDS_BIT, which would flush
    // this thread's context and not the one holding the fence's commands.
    //
    // Elapsed-since-start rather than a precomputed deadline:
    // std::chrono::nanoseconds has a SIGNED 64-bit representation, so
    // nanoseconds(UINT64_MAX) wraps to -1ns and now() + that lands in the past,
    // turning "wait indefinitely" into "give up after one poll". Comparing
    // elapsed time also avoids overflowing the time_point on a large timeout.
    const auto start     = std::chrono::steady_clock::now();
    const bool unbounded = (timeoutNanos == UINT64_MAX);

    while (true)
    {
        const GLenum result = glClientWaitSync(target, 0, 1000000); // 1ms

        if (result == GL_ALREADY_SIGNALED || result == GL_CONDITION_SATISFIED)
        {
            return true;
        }
        if (result == GL_WAIT_FAILED)
        {
            TF_WARN("RenderThreadFence::Wait: glClientWaitSync reported "
                    "GL_WAIT_FAILED. The sync object is not valid on this "
                    "context, which means the contexts are not sharing.");
            return false;
        }

        // GL_TIMEOUT_EXPIRED.
        if (!unbounded)
        {
            const auto elapsed = std::chrono::duration_cast<std::chrono::nanoseconds>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
            if (elapsed >= 0 && static_cast<uint64_t>(elapsed) >= timeoutNanos)
            {
                return false;
            }
        }

        // Avoid a tight CPU spin.
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }
}

} // namespace MAYAHYDRA_NS_DEF
