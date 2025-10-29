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
#ifndef MH_PICK_HIT_H
#define MH_PICK_HIT_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydra.h>

#include <pxr/imaging/hdx/pickTask.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

/// \class PickContext
///
/// Provides an interface that pick handlers can call to obtain information
/// needed to implement picking.
///
struct PickHit
{
    PickHit(int passIdx, HdxPickHit pickHit)
        : passIndex(passIdx)
        , hdxPickHit(pickHit) {}

    const int        passIndex; // FramePass index.
    const HdxPickHit hdxPickHit;
};

}

#endif
