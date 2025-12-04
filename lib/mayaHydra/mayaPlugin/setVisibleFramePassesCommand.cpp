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

#include "renderOverride.h"
#include "setVisibleFramePassesCommand.h"

#include <maya/MArgParser.h>
#include <maya/MIntArray.h>
#include <maya/MSyntax.h>
#include <maya/M3dView.h>
#include <maya/MArgList.h>

#include <pxr/pxr.h>

#include <vector>
#include <algorithm>
#include <utility>
#include <map>

/**
 * MayaHydraSetVisibleFramePasses Command
 * 
 * This command controls which render passes are visible in Maya's Hydra viewport and 
 * optionally specifies the AOV (Arbitrary Output Variable) name.
 * 
 * Functionality:
 * - Set which render passes are visible by specifying pass indices (0, 1, 2, etc.)
 * - Optionally specify the AOV name (defaults to "color" if not provided)
 * - Query currently visible render passes and AOV name
 * - Supports multiple passes in a single command
 * 
 * The command manages static arrays that store the visible pass indices and 
 * the AOV name, which can be accessed by other parts of the Maya-Hydra system.
 */

/* Examples
    //Set only the second render pass visible (uses default "color" AOV)
    MEL : mayaHydraSetVisibleFramePasses -e -v 1
    Python : maya.cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=1)

    //Set only the second render pass visible with "depth" AOV
    MEL : mayaHydraSetVisibleFramePasses -e -v 1 -aov "depth"
    Python : maya.cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=1, aovName="depth")

    //Set both passes visible with default AOV
    MEL : mayaHydraSetVisibleFramePasses -e -v 0 -v 1
    Python : //Python : cmds.mayaHydraSetVisibleFramePasses(edit=True, visible=[0, 1])

    // Query the currently visible render passes and the AOV name
    MEL : mayaHydraSetVisibleFramePasses -q -v
    Python : result = cmds.mayaHydraSetVisibleFramePasses(query=True, visible=True)

    // List available AOV names for a render pass
    MEL : mayaHydraSetVisibleFramePasses -la 0
    Python : result = cmds.mayaHydraSetVisibleFramePasses(listAovs=0)

    // Reset the displayed render passes and AOV to defaults
    MEL : mayaHydraSetVisibleFramePasses -r
    Python : result = cmds.mayaHydraSetVisibleFramePasses(reset=True)
*/

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraSetVisibleFramePasses::commandName("mayaHydraSetVisibleFramePasses");
constexpr int _defaultVisibleFramePasses[] = { 0, 1 }; // Default visible passes (main and second)
const MString _defaultAovName("color");         // Default AOV name

MIntArray MayaHydraSetVisibleFramePasses::_visibleFramePasses(
    _defaultVisibleFramePasses,
    2); // Default visible passes (main and secondary)

MString MayaHydraSetVisibleFramePasses::_aovName(_defaultAovName); // Default AOV name

namespace {

// Array of visible passes with AOV names
constexpr auto _visiblePassesId = "-v";
constexpr auto _visiblePassesIdLong = "-visible";
constexpr auto _aovNameId = "-aov";
constexpr auto _aovNameIdLong = "-aovName";
constexpr auto _listAovsId = "-la";
constexpr auto _listAovsIdLong = "-listAovs";
constexpr auto _resetId = "-r";
constexpr auto _resetIdLong = "-reset";

} // namespace

MSyntax MayaHydraSetVisibleFramePasses::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);
    syntax.enableEdit(true);

     // Add flag for visible passes - requires pass index (int)
    syntax.addFlag(_visiblePassesId, _visiblePassesIdLong, MSyntax::kLong);
    syntax.makeFlagMultiUse(_visiblePassesId);
    
    // Add flag for AOV names - requires AOV name (string)
    syntax.addFlag(_aovNameId, _aovNameIdLong, MSyntax::kString);

    // Add flag to list available AOVs - requires pass index (int)
    syntax.addFlag(_listAovsId, _listAovsIdLong, MSyntax::kUnsigned);

    // Add flag to reset displayed render passes and AOVs
    syntax.addFlag(_resetId, _resetIdLong, MSyntax::kNoArg);

    return syntax;
}

MStatus MayaHydraSetVisibleFramePasses::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st)
        return st;

    const bool isQuery = argData.isQuery();
    const bool isEdit = argData.isEdit();

    if (argData.isFlagSet(_resetId)) {
        _visibleFramePasses = MIntArray(_defaultVisibleFramePasses, sizeof(_defaultVisibleFramePasses) / sizeof(_defaultVisibleFramePasses[0]));
        _aovName = _defaultAovName;
    }

    if (argData.isFlagSet(_listAovsId)) {
        int passIndex = argData.flagArgumentInt(_listAovsId, 0);
        auto aovs = MtohRenderOverride::GetAvailableFramePassAovs(passIndex);
        for (const auto& aov : aovs) {
            appendToResult(aov.GetText());
        }
    }

    if (argData.isFlagSet(_visiblePassesId)) {
        if (isQuery) {
            // Return the currently visible passes and the AOV name
            for (unsigned int i = 0; i < _visibleFramePasses.length(); i++) {
                appendToResult(_visibleFramePasses[i]);
            }
            appendToResult(_aovName);
        } else if (isEdit) {
            _visibleFramePasses.clear();

            // Create a set to store passes for sorting and ensure uniqueness
            // This ensures that pass indices are unique and ordered by increasing pass index
            // Example: "-v 1 -aov color -v 0" will result in 
            //          passes [0,1] with AOV "color"
            std::set<int> passes;

            // Get the pass indices
            const unsigned int numPasses = argData.numberOfFlagUses(_visiblePassesId);

            for (unsigned int i = 0; i < numPasses; ++i) {
                MArgList flagArgs;
                argData.getFlagArgumentList(_visiblePassesId, i, flagArgs);
                if (flagArgs.length() >= 1) {
                    const int passIndex = flagArgs.asInt(0);
                    // Insert into set (automatically handles uniqueness and sorting)
                    passes.insert(passIndex);
                }
            }

            // Get the AOV name
            MString aovName(_defaultAovName);
            argData.getFlagArgument(_aovNameId, 0, aovName);

            // Extract sorted and unique pass indices into member arrays
            // The set automatically keeps entries sorted by key (pass index)
            for (const auto& pass : passes) {
                _visibleFramePasses.append(pass);
            }
            _aovName = aovName;
        }
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
