//
// Copyright 2025 Autodesk
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

#include "testingCommand.h"

#include "renderOverride.h"

#include <maya/MArgList.h>
#include <maya/MArgParser.h>
#include <maya/MSyntax.h>

#include <pxr/pxr.h>

/**
 * mayaHydraTesting command
 * 
 * This command is used to contain functionality dedicated for unit testing. It can for example
 * be used to interface with the main mayaHydra code base to query the state of things.
 * 
 * Current functionality:
 * - Cnecking for convergence
 */

/* Examples
    // Query convergence for a given renderer
    MEL : mayaHydraTesting -cv -rn "HdStormRendererPlugin"
    Python : maya.cmds.mayaHydraTesting(converged=True, rendererName="HdStormRendererPlugin")
*/

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraTestingCommand::commandName("mayaHydraTesting");

namespace {

// Flags
constexpr auto kConverged = "-cv";
constexpr auto kConvergedLong = "-converged";
constexpr auto kRendererName = "-rn";
constexpr auto kRendererNameLong = "-rendererName";

} // namespace

MSyntax MayaHydraTestingCommand::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);

    syntax.addFlag(kConverged, kConvergedLong);

    syntax.addFlag(kRendererName, kRendererNameLong, MSyntax::kString);

    return syntax;
}

MStatus MayaHydraTestingCommand::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st) {
        return st;
    }

    if (argData.isFlagSet(kConverged)) {
        std::vector<MString> activeRendererNames = MtohRenderOverride::AllActiveRendererNames();
        if (activeRendererNames.empty()) {
            displayError("There are no active Hydra renderers.", true);
            return MS::kFailure;
        }

        MString rendererName;
        if (argData.isFlagSet(kRendererName, &st)) {
            if (!st) { return st; }
            rendererName = argData.flagArgumentString(kRendererName, 0, &st);
            if (!st) { return st; }
        }

        if (!rendererName.isEmpty()) {
            TfToken rendererNameToken(rendererName.asChar());
            setResult(MtohRenderOverride::HasConverged(rendererNameToken));
        } else {
            // Check all renderers
            bool haveConverged = true;
            for (const auto& activeRendererName : activeRendererNames) {
                TfToken rendererNameToken(activeRendererName.asChar());
                haveConverged = haveConverged && MtohRenderOverride::HasConverged(rendererNameToken);
            }
            setResult(haveConverged);
        }
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
