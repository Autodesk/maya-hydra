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

#ifndef MAYAHYDRA_SET_VIEWPORT_FRAME_PASSES_CMD_H
#define MAYAHYDRA_SET_VIEWPORT_FRAME_PASSES_CMD_H

#include <mayaHydraLib/mayaHydra.h>
#include <maya/MPxCommand.h>
#include <maya/MIntArray.h>
#include <maya/MStringArray.h>

namespace MAYAHYDRA_NS_DEF {

class MayaHydraSetVisibleFramePasses : public MPxCommand
{
public:
    static void*   creator() { return new MayaHydraSetVisibleFramePasses(); }
    static MSyntax createSyntax();

    static const MString commandName;

    MStatus doIt(const MArgList& args) override;

    static const MIntArray& getVisibleFramePasses() { return _visibleFramePasses; }
    static const MStringArray& getAovNames() { return _aovNames; }

private:
    static MIntArray _visibleFramePasses;
    static MStringArray _aovNames;
};

} // namespace MAYAHYDRA_NS_DEF
#endif // MAYAHYDRA_SET_VIEWPORT_FRAME_PASSES_CMD_H
