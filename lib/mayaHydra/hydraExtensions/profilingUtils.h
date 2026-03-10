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

#ifndef MAYAHYDRALIB_PROFILING_UTILS_H
#define MAYAHYDRALIB_PROFILING_UTILS_H

#include <mayaHydraLib/api.h>

#include <pxr/base/trace/trace.h>
#include <pxr/base/arch/functionLite.h>
#include <pxr/base/tf/preprocessorUtilsLite.h>

#include <maya/MProfiler.h>

#include <string>

/// Utilities for managing both the Maya profiler and USD tracing system simultaneously.
/// After profiling is done, profiling results for Maya can be viewed by opening the profiling
/// window under Window > General Editors > Profiler.
/// The USD tracing system produces .json files following the Chrome tracing format, which
/// can be viewed and analyzed through a tool like https://ui.perfetto.dev/
/// More info can be found in the official docs : https://openusd.org/release/api/trace_page_front.html

namespace MAYAHYDRA_NS_DEF {

/// Returns the MProfiler category ID registered for MayaHydra.
/// The category is lazily created on first call.
MAYAHYDRALIB_API
int ProfilingCategory();

/// Enables both the Maya profiler (MProfiler) and USD trace collector.
MAYAHYDRALIB_API
void StartProfiling();

/// Disables both the Maya profiler and USD trace collector, then writes
/// the collected USD trace data to \p chromeTraceFile in Chrome tracing format.
/// If \p chromeTraceFile is empty, the trace data is not written out.
/// The trace file can be visualized and analyzed with a tool like https://ui.perfetto.dev/
MAYAHYDRALIB_API
void StopProfiling(const std::string& chromeTraceFile);

/// Returns true if either the Maya profiler is active or the USD trace collector
/// is enabled.
MAYAHYDRALIB_API
bool IsProfilingActive();

} // namespace MAYAHYDRA_NS_DEF

#if !defined(MH_PROFILING_ENABLED)
    #define MH_PROFILING_ENABLED 1
#endif

#if MH_PROFILING_ENABLED

/// Profiles the enclosing function in both Maya MProfiler and USD Trace,
/// using the function name as the event/scope key.
#define MH_PROFILE_FUNCTION() \
    TRACE_FUNCTION(); \
    MProfilingScope TF_PP_CAT(_mayaHydraProfiling_, __LINE__)( \
        MAYAHYDRA_NS::ProfilingCategory(), \
        MProfiler::kColorC_L1, \
        __ARCH_PRETTY_FUNCTION__)

/// Profiles the enclosing scope in both Maya MProfiler and USD Trace,
/// using \p name (a string literal) as the event/scope key.
#define MH_PROFILE_SCOPE(name) \
    TRACE_SCOPE(name); \
    MProfilingScope TF_PP_CAT(_mayaHydraProfiling_, __LINE__)( \
        MAYAHYDRA_NS::ProfilingCategory(), \
        MProfiler::kColorB_L3, \
        name)

#else

#define MH_PROFILE_FUNCTION()
#define MH_PROFILE_SCOPE(name)

#endif // MH_PROFILING_ENABLED

#endif // MAYAHYDRALIB_PROFILING_UTILS_H
