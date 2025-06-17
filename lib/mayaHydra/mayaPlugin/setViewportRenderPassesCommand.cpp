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

/* Examples
    //Set only the secondary render pass visible
    mayaHydraSetVisibleRenderPasses -e -v 1

    //Set both passes visible
    mayaHydraSetVisibleRenderPasses -e -v 0 -v 1

    // query the currently visible render passes
    mayaHydraSetVisibleRenderPasses -q -v
*/

namespace MAYAHYDRA_NS_DEF {

const MString MayaHydraSetVisibleRenderPasses::commandName("mayaHydraSetVisibleRenderPasses");
constexpr int _defaultVisibleRenderPasses[] = { 0, 1 }; // Default visible passes (main and secondary)
MIntArray MayaHydraSetVisibleRenderPasses::_visibleRenderPasses(
    _defaultVisibleRenderPasses,
    2); // Default visible passes (main and secondary)

namespace {

// Array of visible passes
constexpr auto _visiblePassesId = "-v";
constexpr auto _visiblePassesIdLong = "-visible";

void refreshAllViewports()
{
    // Force a redraw of all maya viewports
    unsigned int numViews = M3dView::numberOf3dViews();
    for (unsigned int i = 0; i < numViews; i++) {
        M3dView view;
        M3dView::get3dView(i, view);
        view.refresh();
    }
}

} // namespace

MSyntax MayaHydraSetVisibleRenderPasses::createSyntax()
{
    MSyntax syntax;
    syntax.enableQuery(true);
    syntax.enableEdit(true);

     // Add flag for array of visible passes
    syntax.addFlag(_visiblePassesId, _visiblePassesIdLong, MSyntax::kLong);
    syntax.makeFlagMultiUse(_visiblePassesId);

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
            // Return the currently visible passes
            // Option 1: Add each element individually
            for (unsigned int i = 0; i < _visibleRenderPasses.length(); i++) {
                appendToResult(_visibleRenderPasses[i]);
            }
        } else if (isEdit) {
            _visibleRenderPasses.clear();

            // Then show only the specified passes
            const unsigned int numUses = argData.numberOfFlagUses(_visiblePassesId);
            for (unsigned int i = 0; i < numUses; ++i) {
                MArgList flagArgs;
                argData.getFlagArgumentList(_visiblePassesId, i, flagArgs);

                const int passIndex = flagArgs.asInt(0);
                _visibleRenderPasses.append(passIndex);
            }

            // Refresh viewports once after all changes
            refreshAllViewports();
        }
    }

    return MS::kSuccess;
}

} // namespace MAYAHYDRA_NS_DEF
