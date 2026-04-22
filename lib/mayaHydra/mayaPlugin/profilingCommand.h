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

#ifndef MAYAHYDRA_PROFILING_COMMAND_H
#define MAYAHYDRA_PROFILING_COMMAND_H

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MPxCommand.h>

namespace MAYAHYDRA_NS_DEF {

class MayaHydraProfilingCommand : public MPxCommand
{
public:
    static void*   creator() { return new MayaHydraProfilingCommand(); }
    static MSyntax createSyntax();

    static const MString commandName;

    MStatus doIt(const MArgList& args) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYAHYDRA_PROFILING_COMMAND_H
