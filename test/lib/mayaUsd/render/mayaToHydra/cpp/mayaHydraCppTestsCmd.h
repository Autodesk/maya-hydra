// Copyright 2023 Autodesk
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
#ifndef MH_CPPTESTS_CMD
#define MH_CPPTESTS_CMD

#include <maya/MPxCommand.h>

class mayaHydraCppTestCmd : public MPxCommand
{
public:
    static void*   creator() { return new mayaHydraCppTestCmd(); }
    static MSyntax createSyntax();

    MStatus doIt(const MArgList& args) override;
};

class mayaHydraInstruments : public MPxCommand
{
public:
    static void*   creator() { return new mayaHydraInstruments(); }
    static MSyntax createSyntax();

    MStatus doIt(const MArgList& args) override;
};

#endif // MH_CPPTESTS_CMD
