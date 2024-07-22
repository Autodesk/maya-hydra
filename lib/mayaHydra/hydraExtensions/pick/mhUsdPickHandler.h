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
#ifndef MH_USD_PICK_HANDLER_H
#define MH_USD_PICK_HANDLER_H

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydra.h>
#include <mayaHydraLib/pick/mhPickHandler.h>

#include <pxr/usd/sdf/path.h>

#include <tuple>

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
class MayaHydraSceneIndexRegistry;
PXR_NAMESPACE_CLOSE_SCOPE

namespace MAYAHYDRA_NS_DEF {

/// \class UsdPickHandler
///
/// The pick handler performs the picking to selection mapping for USD data.
/// It places its output in the PickOutput UFE selection.

class UsdPickHandler : public PickHandler {
public:

    UsdPickHandler() = default;

    // Describe a pick hit path.  The SdfPath is in the original data model
    // scene (USD), not in the scene index scene.
    using HitPath = std::tuple<PXR_NS::SdfPath, int>;
    
    UsdPickHandler(HdRenderIndex* renderIndex);

    bool handlePickHit(
        const Input& pickInput, Output& pickOutput
    ) const override;

private:

    PXR_NS::HdRenderIndex* renderIndex() const;
    std::shared_ptr<const MayaHydraSceneIndexRegistry>
    sceneIndexRegistry() const;
};

}

#endif
