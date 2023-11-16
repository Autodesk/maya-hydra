//
// Copyright 2019 Luma Pictures
// Copyright 2023 Autodesk, Inc. All rights reserved.
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
#ifndef MTOH_CMD_H
#define MTOH_CMD_H

#include <mayaHydraLib/mayaHydra.h>

#include <maya/MPxCommand.h>

namespace MAYAHYDRA_NS_DEF {

class MtohViewCmd : public MPxCommand
{
public:
    static void*   creator() { return new MtohViewCmd(); }
    static MSyntax createSyntax();

    static const MString name;

    MStatus doIt(const MArgList& args) override;
};

}

#endif // MTOH_CMD_H
