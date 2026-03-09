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

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraProfilingCommand::commandName("mayaHydraProfiling");

namespace {

constexpr auto kEnabled = "-e";
constexpr auto kEnabledLong = "-enabled";

constexpr auto kUsdTraceFile = "-utf";
constexpr auto kUsdTraceFileLong = "-usdTraceFile";

constexpr auto kIsEnabled = "-ie";
constexpr auto kIsEnabledLong = "-isEnabled";

} // namespace

MSyntax MayaHydraProfilingCommand::createSyntax()
{
    MSyntax syntax;

    syntax.addFlag(kEnabled, kEnabledLong, MSyntax::kBoolean);
    syntax.addFlag(kUsdTraceFile, kUsdTraceFileLong, MSyntax::kString);
    syntax.addFlag(kIsEnabled, kIsEnabledLong);

    return syntax;
}

MStatus MayaHydraProfilingCommand::doIt(const MArgList& args)
{
    MStatus status;

    MArgDatabase db(syntax(), args, &status);
    if (!status) {
        return status;
    }

    if (db.isFlagSet(kEnabled)) {
        bool enabled = false;
        CHECK_MSTATUS_AND_RETURN_IT(db.getFlagArgument(kEnabled, 0, enabled));
        if (enabled) {
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

    if (db.isFlagSet(kIsEnabled)) {
        setResult(IsProfilingActive());
        return MS::kSuccess;
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
