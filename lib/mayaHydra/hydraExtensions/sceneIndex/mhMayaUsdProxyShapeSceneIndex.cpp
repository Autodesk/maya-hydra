//
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

#include "mhMayaUsdProxyShapeSceneIndex.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace MAYAHYDRA_NS_DEF {

MayaUsdProxyShapeSceneIndex::MayaUsdProxyShapeSceneIndex(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath)
    : ParentClass(proxyStage, sceneIndexChainLastElement, usdImagingStageSceneIndex, dagNodeHandle,
                  sceneIndexPathPrefix, sceneIndexAppPath)
{
}

MayaUsdProxyShapeSceneIndex::~MayaUsdProxyShapeSceneIndex() = default;

MayaUsdProxyShapeSceneIndexRefPtr MayaUsdProxyShapeSceneIndex::New(
    const MAYAUSDAPI_NS::ProxyStage&       proxyStage,
    const HdSceneIndexBaseRefPtr&          sceneIndexChainLastElement,
    const UsdImagingStageSceneIndexRefPtr& usdImagingStageSceneIndex,
    const MObjectHandle&                   dagNodeHandle,
    const SdfPath&                         sceneIndexPathPrefix,
    const Ufe::Path&                       sceneIndexAppPath)
{
    return TfCreateRefPtr(new MayaUsdProxyShapeSceneIndex(
        proxyStage,
        sceneIndexChainLastElement,
        usdImagingStageSceneIndex,
        dagNodeHandle,
        sceneIndexPathPrefix,
        sceneIndexAppPath));
}

} // namespace MAYAHYDRA_NS_DEF
