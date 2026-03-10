//
// Copyright 2026 Autodesk
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

#include "profilingCommand.h"

#include <mayaHydraLib/profilingUtils.h>

#include <maya/MArgDatabase.h>
#include <maya/MSyntax.h>

/**
 * mayaHydraProfiling command
 * 
 * This command allows the user to start/stop both the Maya profiler and USD tracing system at the same time.
 * When stopping profiling, a .json file path for where to store the USD tracing results can optionally be passed. 
 * The command also allows for querying whether profiling is currently active or not.
 * Information on the USD tracing system can be found at https://openusd.org/release/api/trace_page_front.html
 * USD traces can be visualized and analyzed with a tool like https://ui.perfetto.dev/
 * 
 * Functionality:
 * - Start/Stop Maya and USD profiling
 * - Set where to store USD tracing results
 * - Query if profiling is active
 */

/* Examples
    // Start profiling
    MEL : mayaHydraProfiling -a 1
    Python : maya.cmds.mayaHydraProfiling(active=True)

    // Stop profiling
    MEL : mayaHydraProfiling -a 0 -utf "D:/dev/traces/mayaHydraUsdTrace.json"
    Python : maya.cmds.mayaHydraProfiling(active=False, usdTraceFile="D:/dev/traces/mayaHydraUsdTrace.json")

    // Query if profiling is active
    MEL : mayaHydraProfiling -ia
    Python : maya.cmds.mayaHydraProfiling(isActive=True)
*/

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraProfilingCommand::commandName("mayaHydraProfiling");

namespace {

constexpr auto kActive = "-a";
constexpr auto kActiveLong = "-active";

constexpr auto kUsdTraceFile = "-utf";
constexpr auto kUsdTraceFileLong = "-usdTraceFile";

constexpr auto kIsActive = "-ia";
constexpr auto kIsActiveLong = "-isActive";

} // namespace

MSyntax MayaHydraProfilingCommand::createSyntax()
{
    MSyntax syntax;

    syntax.addFlag(kActive, kActiveLong, MSyntax::kBoolean);
    syntax.addFlag(kUsdTraceFile, kUsdTraceFileLong, MSyntax::kString);
    syntax.addFlag(kIsActive, kIsActiveLong);

    return syntax;
}

MStatus MayaHydraProfilingCommand::doIt(const MArgList& args)
{
    MStatus status;

    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    if (db.isFlagSet(kActive)) {
        bool active = false;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(kActive, 0, active));
        if (active) {
            StartProfiling();
        } else {
            MString filePath;
            if (db.isFlagSet(kUsdTraceFile)) {
                CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(kUsdTraceFile, 0, filePath));
            }
            StopProfiling(filePath.asChar());
        }
        return MS::kSuccess;
    }

    if (db.isFlagSet(kIsActive)) {
        setResult(IsProfilingActive());
        return MS::kSuccess;
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
