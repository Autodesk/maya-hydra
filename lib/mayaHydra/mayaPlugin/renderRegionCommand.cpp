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

#include "renderRegionCommand.h"

#include <maya/MArgList.h>
#include <maya/MArgParser.h>
#include <maya/MSyntax.h>

/**
 * mayaHydraRenderRegion command
 * 
 * This command allows the user to specify a specific sub-region of the viewport
 * in which the chosen renderer is expected to render.
 * 
 * Functionality:
 * - Set the desired render region
 * - Clear the current render region
 * - Query the current render region
 */

/* Examples
    // Set the render region
    MEL : mayaHydraRenderRegion -e -r 20 50 700 800
    Python : maya.cmds.mayaHydraRenderRegion(edit=True, region=[20, 50, 700, 800])

    // Clear the render region
    MEL : mayaHydraRenderRegion -e -c
    Python : maya.cmds.mayaHydraRenderRegion(edit=True, clear=True)

    // Query the render region
    MEL : mayaHydraRenderRegion -q -r
    Python : maya.cmds.mayaHydraRenderRegion(query=True, region=True)
*/

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraRenderRegionCommand::commandName("mayaHydraRenderRegion");

std::optional<GfRect2i> MayaHydraRenderRegionCommand::_renderRegion;

namespace {

// Flags
constexpr auto kRegion = "-r";
constexpr auto kRegionLong = "-region";
constexpr auto kClear = "-c";
constexpr auto kClearLong = "-clear";

} // namespace

MSyntax MayaHydraRenderRegionCommand::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);
    syntax.enableEdit(true);

    syntax.addFlag(kRegion, kRegionLong, 
        MSyntax::kUnsigned, MSyntax::kUnsigned, MSyntax::kUnsigned, MSyntax::kUnsigned);
    
    syntax.addFlag(kClear, kClearLong);

    return syntax;
}

MStatus MayaHydraRenderRegionCommand::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st)
        return st;

    const bool isQuery = argData.isQuery();
    const bool isEdit = argData.isEdit();

    if (argData.isFlagSet(kClear) && isEdit) {
        _renderRegion = GfRect2i();
        _renderRegion.reset();
    }

    if (argData.isFlagSet(kRegion)) {
        if (isQuery) {
            if (_renderRegion.has_value()) {
                appendToResult(_renderRegion->GetMinX());
                appendToResult(_renderRegion->GetMinY());
                appendToResult(_renderRegion->GetMaxX());
                appendToResult(_renderRegion->GetMaxY());
            } else {
                displayError("Tried to query render region, but none was defined.", true);
                return MS::kFailure;
            }
        } else if (isEdit) {
            _renderRegion = GfRect2i();
            _renderRegion->SetMinX(argData.flagArgumentInt(kRegion, 0));
            _renderRegion->SetMinY(argData.flagArgumentInt(kRegion, 1));
            _renderRegion->SetMaxX(argData.flagArgumentInt(kRegion, 2));
            _renderRegion->SetMaxY(argData.flagArgumentInt(kRegion, 3));
        }
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
