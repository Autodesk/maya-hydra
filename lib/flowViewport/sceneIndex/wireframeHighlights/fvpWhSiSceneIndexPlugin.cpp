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

#include "fvpWhSiSceneIndexPlugin.h"

#include "fvpGenerativeProceduralWhSi.h"
#include "fvpGeomSubsetWhSi.h"
#include "fvpMeshWhSi.h"
#include "fvpNiInstanceWhSi.h"
#include "fvpNiPrototypeWhSi.h"
#include "fvpPiInstancerWhSi.h"
#include "fvpPiPrototypeWhSi.h"

#include <pxr/imaging/hd/sceneIndexPluginRegistry.h>

#include <flowViewport/fvpDeferredWireframeColorInterface.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
const SdfPath kMayaNativeRoot("/MayaHydraViewportRenderer");
const SdfPath kHighlightHierarchyPrefix("/FlowViewportSelectionHighlights");

auto deferredWci = std::make_shared<Fvp::DeferredWireframeColorInterface>();
} // anonymous namespace

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(HdSceneIndexPlugin)
{
    constexpr int kWhSiInsertionPhase = 3;

    HdSceneIndexPluginRegistry::GetInstance().RegisterSceneIndexForRenderer(
        std::string(), // all renderers
        [](const std::string& /*renderInstanceId*/,
           const HdSceneIndexBaseRefPtr& inputScene,
           const HdContainerDataSourceHandle& /*inputArgs*/) -> HdSceneIndexBaseRefPtr {
            if (!inputScene) {
                return inputScene;
            }

            std::shared_ptr<Fvp::WireframeColorInterface> wireframeColorInterface = deferredWci;
            HdSceneIndexBaseRefPtr                        lastSceneIndex = inputScene;

#if PXR_VERSION >= 2405
            auto geomSubsetWhSi = Fvp::GeomSubsetWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            geomSubsetWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = geomSubsetWhSi;
#endif

            auto meshWhSi = Fvp::MeshWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            meshWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = meshWhSi;

            auto niInstanceWhSi = Fvp::NiInstanceWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            niInstanceWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = niInstanceWhSi;

            auto niPrototypeWhSi
                = Fvp::NiPrototypeWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            niPrototypeWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = niPrototypeWhSi;

            auto piInstancerWhSi
                = Fvp::PiInstancerWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            piInstancerWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = piInstancerWhSi;

            auto piPrototypeWhSi
                = Fvp::PiPrototypeWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            piPrototypeWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = piPrototypeWhSi;

            auto gpWhSi
                = Fvp::GenerativeProceduralWhSi::New(lastSceneIndex, kHighlightHierarchyPrefix, wireframeColorInterface);
            gpWhSi->AddExcludedPath(kMayaNativeRoot);
            lastSceneIndex = gpWhSi;

            return lastSceneIndex;
        },
        nullptr, // inputArgs
        kWhSiInsertionPhase,
        HdSceneIndexPluginRegistry::InsertionOrderAtEnd);
}

PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

void SetWhSiWireframeColorInterface(const std::shared_ptr<WireframeColorInterface>& wci)
{
    if (deferredWci) {
        deferredWci->SetImplementation(wci);
    }
}

} // namespace FVP_NS_DEF