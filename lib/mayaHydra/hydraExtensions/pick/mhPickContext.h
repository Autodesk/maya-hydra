//
// Copyright 2024 Autodesk
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
#ifndef MH_PICK_CONTEXT_H
#define MH_PICK_CONTEXT_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydra.h>

#include <pxr/pxr.h>

#include <memory>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
class MayaHydraSceneIndexRegistry;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/// \class PickContext
///
/// Provides an interface that pick handlers can call to obtain information
/// needed to implement picking.
///
class PickContext
{
public:

    MAYAHYDRALIB_API
    virtual std::shared_ptr<const PXR_NS::MayaHydraSceneIndexRegistry>
    sceneIndexRegistry() const = 0;

    MAYAHYDRALIB_API
    virtual PXR_NS::HdRenderIndex* renderIndex() const = 0;
};

}

#endif
