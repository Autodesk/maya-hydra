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

#include "getFramePassesCountCommand.h"
#include "envSettings.h"
#include "renderOverride.h"

#include <maya/MArgParser.h>
#include <maya/MSyntax.h>
#include <maya/MArgList.h>

/**
 * mayaHydraGetFramePassesCount Command
 * 
 * This command returns the number of frame passes that are currently created for the specified renderer.
 * It queries the actual MtohRenderOverride instance to get the real number of frame passes.
 * 
 * Functionality:
 * - Returns the actual number of frame passes from the specified renderer
 * - If no renderer name is provided, uses the first active renderer (default behavior)
 * - Queries the MtohRenderOverride instance directly
 * - Returns 0 if no active renderer is found or if the specified renderer is not active
 * 
 * Arguments:
 *     -rn/-rendererName (string): Optional renderer name to query. If not provided, uses the first active renderer.
 * 
 * Examples:
 *     // Query the number of frame passes for the first active renderer
 *     MEL : mayaHydraGetFramePassesCount()
 *     Python : maya.cmds.mayaHydraGetFramePassesCount()
 *     
 *     // Query the number of frame passes for a specific renderer
 *     MEL : mayaHydraGetFramePassesCount(-rendererName "HdStormRendererPlugin")
 *     Python : maya.cmds.mayaHydraGetFramePassesCount(rendererName="HdStormRendererPlugin")
 */

using namespace PXR_NS;

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraGetFramePassesCount::commandName("mayaHydraGetFramePassesCount");
constexpr auto kRendererNameShort = "-rn";
constexpr auto kRendererNameLong = "-rendererName";

MSyntax MayaHydraGetFramePassesCount::createSyntax()
{
    MSyntax syntax;
    // Add optional renderer name argument
    syntax.addFlag(kRendererNameShort, kRendererNameLong, MSyntax::kString);
    return syntax;
}

MStatus MayaHydraGetFramePassesCount::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st)
        return st;

    // Check if renderer name is provided
    MString rendererName;
    if (argData.isFlagSet(kRendererNameShort, &st)) {
        if (st != MS::kSuccess) {
            return st;
        }
        rendererName = argData.flagArgumentString(kRendererNameShort, 0, &st);
        if (st != MS::kSuccess) {
            return st;
        }
    }

    // Return the number of frame passes
    setResult(getFramePassesCount(rendererName));

    return MS::kSuccess;
}

int MayaHydraGetFramePassesCount::getFramePassesCount(const MString& rendererName)
{
    // Get all active renderer names
    std::vector<MString> activeRenderers = MtohRenderOverride::AllActiveRendererNames();
    
    if (activeRenderers.empty()) {
        return 0; // No active renderers
    }
    
    MString targetRendererName;
    
    if (rendererName.length() > 0) {
        // Use the provided renderer name
        targetRendererName = rendererName;
        
        // Check if the specified renderer is active
        bool rendererFound = false;
        for (const auto& activeRenderer : activeRenderers) {
            if (activeRenderer == targetRendererName) {
                rendererFound = true;
                break;
            }
        }
        
        if (!rendererFound) {
            return 0; // Specified renderer is not active
        }
    } else {
        // Use the first active renderer (default behavior)
        targetRendererName = activeRenderers[0];
    }
    
    // Get the MtohRenderOverride instance for this renderer
    TfToken rendererToken(targetRendererName.asChar());
    MtohRenderOverride* renderOverride = MtohRenderOverride::GetByName(rendererToken);
    
    if (!renderOverride) {
        return 0; // No render override found
    }
    
    // Return the actual number of frame passes
    return renderOverride->getNumFramePasses();
}

} // namespace MAYAHYDRA_NS_DEF
