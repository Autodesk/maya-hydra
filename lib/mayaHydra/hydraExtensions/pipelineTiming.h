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
//
// Controlled by the MAYAHYDRA_PIPELINE_TIMING env flag (default off). When on,
// each MH_PIPELINE_TIMING_SCOPE emits one stderr line per stage per frame,
//   PIPELINE_TIMING <stage> <elapsed_ms> <frame_id>
// so an external harness can capture it and compare Hydra against VP2
// sessions. Setting MH_PIPELINE_TIMING_ENABLED to 0 removes the macros.

#ifndef MAYAHYDRA_PIPELINE_TIMING_H
#define MAYAHYDRA_PIPELINE_TIMING_H

#include <mayaHydraLib/api.h>

#include <pxr/base/tf/preprocessorUtilsLite.h>
#include <pxr/base/arch/functionLite.h>

#include <chrono>
#include <cstdio>
#include <atomic>
#include <cstdint>

namespace MAYAHYDRA_NS_DEF {

/// True when MAYAHYDRA_PIPELINE_TIMING is set. Cached on first call.
MAYAHYDRALIB_API
bool IsPipelineTimingEnabled();

/// Monotonic frame id used to correlate the timing lines of one frame.
MAYAHYDRALIB_API
uint64_t GetTimingFrameId();

/// Called once per render op entry.
MAYAHYDRALIB_API
void IncrementTimingFrameId();

/// RAII scope timer. Emits the elapsed wall-clock milliseconds to stderr on
/// destruction when timing is enabled.
class MAYAHYDRALIB_API PipelineTimingScope
{
public:
    /// \p stageName A short label, e.g. "EM_eval", "SyncAll_Execute",
    /// "SubmitCmds", "present".
    explicit PipelineTimingScope(const char* stageName);
    ~PipelineTimingScope();

    PipelineTimingScope(const PipelineTimingScope&) = delete;
    PipelineTimingScope& operator=(const PipelineTimingScope&) = delete;

private:
    const char*                                   _stageName;
    std::chrono::steady_clock::time_point         _start;
    bool                                          _enabled;
};

} // namespace MAYAHYDRA_NS_DEF

#if !defined(MH_PIPELINE_TIMING_ENABLED)
    #define MH_PIPELINE_TIMING_ENABLED 1
#endif

#if MH_PIPELINE_TIMING_ENABLED

/// Times the enclosing scope.
#define MH_PIPELINE_TIMING_SCOPE(name) \
    MAYAHYDRA_NS_DEF::PipelineTimingScope \
    TF_PP_CAT(_pipelineTimingScope, __LINE__)(name)

/// Times the enclosing function.
#define MH_PIPELINE_TIMING_FUNCTION() \
    MH_PIPELINE_TIMING_SCOPE(__ARCH_PRETTY_FUNCTION__)

#else

#define MH_PIPELINE_TIMING_SCOPE(name)
#define MH_PIPELINE_TIMING_FUNCTION()

#endif // MH_PIPELINE_TIMING_ENABLED

#endif // MAYAHYDRA_PIPELINE_TIMING_H
