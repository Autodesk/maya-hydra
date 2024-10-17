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

#include "fvpPrimUtils.h"

#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

bool isPointInstancer(
    const HdSceneIndexBaseRefPtr& si,
    const SdfPath&                primPath
)
{
    return isPointInstancer(si->GetPrim(primPath));
}

bool isPointInstancer(const PXR_NS::HdSceneIndexPrim& prim)
{
    // If prim isn't an instancer, it can't be a point instancer.
    if (prim.primType != HdPrimTypeTokens->instancer) {
        return false;
    }

    HdInstancerTopologySchema instancerTopologySchema = HdInstancerTopologySchema::GetFromParent(prim.dataSource);

    // Documentation
    // https://github.com/PixarAnimationStudios/OpenUSD/blob/59992d2178afcebd89273759f2bddfe730e59aa8/pxr/imaging/hd/instancerTopologySchema.h#L86
    // says that instanceLocations is only meaningful for native
    // instancing, empty for point instancing.
    auto instanceLocationsDs = instancerTopologySchema.GetInstanceLocations();
    if (!instanceLocationsDs) {
        return true;
    }

    auto instanceLocations = instanceLocationsDs->GetTypedValue(0.0f);

    return (instanceLocations.size() > 0);
}

} // namespace FVP_NS_DEF
