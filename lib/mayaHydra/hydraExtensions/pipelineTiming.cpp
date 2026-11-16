//
// Copyright 2026 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Per-frame pipeline timing instrumentation.

#include "pipelineTiming.h"

#include <pxr/base/tf/envSetting.h>
#include <pxr/base/arch/api.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_DEFINE_ENV_SETTING(
    MAYAHYDRA_PIPELINE_TIMING,
    false,
    "Enable per-frame pipeline timing instrumentation. When true, emits "
    "PIPELINE_TIMING <stage> <elapsed_ms> <frame_id> lines to stderr for "
    "each pipeline stage. Default: false.");

PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

bool IsPipelineTimingEnabled()
{
    static const bool _enabled = PXR_NS::TfGetEnvSetting(PXR_NS::MAYAHYDRA_PIPELINE_TIMING);
    return _enabled;
}

static std::atomic<uint64_t> s_timingFrameId{0};

uint64_t GetTimingFrameId()
{
    return s_timingFrameId.load(std::memory_order_relaxed);
}

void IncrementTimingFrameId()
{
    s_timingFrameId.fetch_add(1, std::memory_order_relaxed);
}

PipelineTimingScope::PipelineTimingScope(const char* stageName)
    : _stageName(stageName)
    , _start(std::chrono::steady_clock::now())
    , _enabled(IsPipelineTimingEnabled())
{
}

PipelineTimingScope::~PipelineTimingScope()
{
    if (!_enabled) {
        return;
    }

    auto end = std::chrono::steady_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(
        end - _start).count();
    double elapsedMs = static_cast<double>(elapsedUs) / 1000.0;
    uint64_t frameId = s_timingFrameId.load(std::memory_order_relaxed);

    std::fprintf(stderr,
                 "PIPELINE_TIMING %s %.3f %llu\n",
                 _stageName,
                 elapsedMs,
                 static_cast<unsigned long long>(frameId));
}

} // namespace MAYAHYDRA_NS_DEF
