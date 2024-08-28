//
// Copyright 2024 Autodesk
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

#include "pluginBuildInfoCommand.h"

#include <mayaHydraLib/mhBuildInfo.h>

#include <maya/MArgParser.h>
#include <maya/MSyntax.h>

#define XSTR(x) STR(x)
#define STR(x)  #x

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraPluginInfoCommand::commandName("mayaHydraBuildInfo");

namespace {

// Versioning and build information.
constexpr auto kMajorVersion = "-mjv";
constexpr auto kMajorVersionLong = "-majorVersion";

constexpr auto kMinorVersion = "-mnv";
constexpr auto kMinorVersionLong = "-minorVersion";

constexpr auto kPatchVersion = "-pv";
constexpr auto kPatchVersionLong = "-patchVersion";

constexpr auto kVersion = "-v";
constexpr auto kVersionLong = "-version";

constexpr auto kCutId = "-c";
constexpr auto kCutIdLong = "-cutIdentifier";

constexpr auto kBuildNumber = "-bn";
constexpr auto kBuildNumberLong = "-buildNumber";

constexpr auto kGitCommit = "-gc";
constexpr auto kGitCommitLong = "-gitCommit";

constexpr auto kGitBranch = "-gb";
constexpr auto kGitBranchLong = "-gitBranch";

constexpr auto kBuildDate = "-bd";
constexpr auto kBuildDateLong = "-buildDate";

} // namespace

MSyntax MayaHydraPluginInfoCommand::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(false);
    syntax.enableEdit(false);

    // Versioning and build information flags.
    syntax.addFlag(kMajorVersion, kMajorVersionLong);
    syntax.addFlag(kMinorVersion, kMinorVersionLong);
    syntax.addFlag(kPatchVersion, kPatchVersionLong);
    syntax.addFlag(kVersion, kVersionLong);
    syntax.addFlag(kCutId, kCutIdLong);
    syntax.addFlag(kBuildNumber, kBuildNumberLong);
    syntax.addFlag(kGitCommit, kGitCommitLong);
    syntax.addFlag(kGitBranch, kGitBranchLong);
    syntax.addFlag(kBuildDate, kBuildDateLong);

    return syntax;
}

MStatus MayaHydraPluginInfoCommand::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st)
        return st;

    if (argData.isFlagSet(kMajorVersion)) {
        setResult(MAYAHYDRA_MAJOR_VERSION); // int
    } else if (argData.isFlagSet(kMinorVersion)) {
        setResult(MAYAHYDRA_MINOR_VERSION); // int
    } else if (argData.isFlagSet(kPatchVersion)) {
        setResult(MAYAHYDRA_PATCH_LEVEL); // int
    } else if (argData.isFlagSet(kVersion)) {
        setResult(XSTR(MAYAHYDRA_VERSION)); // convert to string
    } else if (argData.isFlagSet(kCutId)) {
        setResult(MhBuildInfo::cutId());
    } else if (argData.isFlagSet(kBuildNumber)) {
        setResult(MhBuildInfo::buildNumber());
    } else if (argData.isFlagSet(kGitCommit)) {
        setResult(MhBuildInfo::gitCommit());
    } else if (argData.isFlagSet(kGitBranch)) {
        setResult(MhBuildInfo::gitBranch());
    } else if (argData.isFlagSet(kBuildDate)) {
        setResult(MhBuildInfo::buildDate());
    }

    return MS::kSuccess;
}

} // namespace MAYAUSD_NS_DEF
