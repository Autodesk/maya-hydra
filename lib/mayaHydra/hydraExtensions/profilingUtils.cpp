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

#include "profilingUtils.h"

#include <pxr/base/tf/diagnostic.h>
#include <pxr/base/trace/collector.h>
#include <pxr/base/trace/reporter.h>

#include <maya/MProfiler.h>

#include <fstream>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

int ProfilingCategory()
{
    static int category = MProfiler::addCategory("MayaHydra", "Events from MayaHydra profiling utilities");
    return category;
}

void StartProfiling()
{
    TraceCollector::GetInstance().SetEnabled(true);
    MProfiler::setRecordingActive(true);
}

void StopProfiling(const std::string& chromeTraceFile)
{
    TraceCollector::GetInstance().SetEnabled(false);
    MProfiler::setRecordingActive(false);

    if (!chromeTraceFile.empty()) {
        std::ofstream out(chromeTraceFile);
        if (out.is_open()) {
            TraceReporter::GetGlobalReporter()->ReportChromeTracing(out);
        }
    }
}

bool IsProfilingActive()
{
    bool isMayaProfilerActive = MProfiler::recordingActive();
    bool isUsdProfilerEnabled = TraceCollector::IsEnabled();
    if (isMayaProfilerActive != isUsdProfilerEnabled) {
        if (isMayaProfilerActive) {
            TF_WARN("Maya profiler is active but USD tracing is disabled");
        } else {
            TF_WARN("USD tracing is enabled but Maya profiler is inactive");
        }
    }
    return isMayaProfilerActive || isUsdProfilerEnabled;
}

} // namespace MAYAHYDRA_NS_DEF
