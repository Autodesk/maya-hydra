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

#include "setViewportRenderPassesCommand.h"

#include <maya/MArgParser.h>
#include <maya/MSyntax.h>
#include <maya/M3dView.h>
#include <maya/MArgList.h>

#include <vector>
#include <algorithm>
#include <utility>
#include <map>

/**
 * mayaHydraSetVisibleRenderPasses Command
 * 
 * This command controls which render passes are visible in Maya's Hydra viewport and 
 * optionally specifies the AOV (Arbitrary Output Variable) name for each pass.
 * 
 * Functionality:
 * - Set which render passes are visible by specifying pass indices (0, 1, 2, etc.)
 * - Optionally specify AOV names for each pass (defaults to "color" if not provided)
 * - Query currently visible render passes and their associated AOV names
 * - Supports multiple passes in a single command
 * 
 * The command manages static arrays that store the visible pass indices and their
 * corresponding AOV names, which can be accessed by other parts of the Maya-Hydra system.
 */

/* Examples
    //Set only the second render pass visible (uses default "color" AOV)
    mayaHydraSetVisibleRenderPasses -e -v 1

    //Set only the second render pass visible with "depth" AOV
    mayaHydraSetVisibleRenderPasses -e -v 1 -aov "depth"

    //Set both passes visible with default AOVs
    mayaHydraSetVisibleRenderPasses -e -v 0 -v 1

    //Set both passes visible with different AOVs
    mayaHydraSetVisibleRenderPasses -e -v 0 -aov "color" -v 1 -aov "depth"

    // query the currently visible render passes and their AOV names
    mayaHydraSetVisibleRenderPasses -q -v
*/

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraSetVisibleRenderPasses::commandName("mayaHydraSetVisibleRenderPasses");
constexpr int _defaultVisibleRenderPasses[] = { 0, 1 }; // Default visible passes (main and second)
const char* _defaultAovNames[] = { "color", "color" }; // Default AOV names

MIntArray MayaHydraSetVisibleRenderPasses::_visibleRenderPasses(
    _defaultVisibleRenderPasses,
    2); // Default visible passes (main and secondary)

MStringArray MayaHydraSetVisibleRenderPasses::_aovNames(
    _defaultAovNames,
    2); // Default AOV names

namespace {

// Array of visible passes with AOV names
constexpr auto _visiblePassesId = "-v";
constexpr auto _visiblePassesIdLong = "-visible";
constexpr auto _aovNameId = "-aov";
constexpr auto _aovNameIdLong = "-aovName";

} // namespace

MSyntax MayaHydraSetVisibleRenderPasses::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);
    syntax.enableEdit(true);

     // Add flag for visible passes - requires pass index (int)
    syntax.addFlag(_visiblePassesId, _visiblePassesIdLong, MSyntax::kLong);
    syntax.makeFlagMultiUse(_visiblePassesId);
    
    // Add flag for AOV names - requires AOV name (string)
    syntax.addFlag(_aovNameId, _aovNameIdLong, MSyntax::kString);
    syntax.makeFlagMultiUse(_aovNameId);

    return syntax;
}

MStatus MayaHydraSetVisibleRenderPasses::doIt(const MArgList& args)
{
    MStatus    st;
    MArgParser argData(syntax(), args, &st);
    if (!st)
        return st;

    const bool isQuery = argData.isQuery();
    const bool isEdit = argData.isEdit();

    if (argData.isFlagSet(_visiblePassesId)) {
        if (isQuery) {
            // Return the currently visible passes and their AOV names
            for (unsigned int i = 0; i < _visibleRenderPasses.length(); i++) {
                appendToResult(_visibleRenderPasses[i]);
                appendToResult(_aovNames[i]);
            }
        } else if (isEdit) {
            _visibleRenderPasses.clear();
            _aovNames.clear();

            // Create a map to store pass-AOV pairs for sorting and ensure uniqueness
            // This ensures that pass indices are unique and ordered by increasing pass index
            // If the same pass index is specified multiple times, the last AOV name will be used
            // Example: "-v 1 -aov color -v 0 -aov depth" will result in 
            //          passes [0,1] with AOVs ["depth","color"]
            std::map<int, MString> passAovMap;

            // Get the pass indices and their corresponding AOV names
            const unsigned int numPasses = argData.numberOfFlagUses(_visiblePassesId);
            const unsigned int numAovs = argData.isFlagSet(_aovNameId) ? argData.numberOfFlagUses(_aovNameId) : 0;

            for (unsigned int i = 0; i < numPasses; ++i) {
                MArgList flagArgs;
                argData.getFlagArgumentList(_visiblePassesId, i, flagArgs);
                if (flagArgs.length() >= 1) {
                    const int passIndex = flagArgs.asInt(0);
                    
                    // Determine the AOV name for this pass
                    MString aovName("color"); // Default AOV
                    if (i < numAovs) {
                        MArgList aovFlagArgs;
                        argData.getFlagArgumentList(_aovNameId, i, aovFlagArgs);
                        if (aovFlagArgs.length() >= 1) {
                            aovName = aovFlagArgs.asString(0);
                        }
                    }
                    
                    // Insert into map (automatically handles uniqueness and sorting)
                    passAovMap[passIndex] = aovName;
                }
            }

            // Extract sorted and unique pass indices and AOV names into member arrays
            // The map automatically keeps entries sorted by key (pass index)
            for (const auto& pair : passAovMap) {
                _visibleRenderPasses.append(pair.first);
                _aovNames.append(pair.second);
            }
        }
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
