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
#ifndef FVP_PRUNE_TEXTURE_SCENE_INDEX_NEW_H
#define FVP_PRUNE_TEXTURE_SCENE_INDEX_NEW_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hdsi/api.h>
#include <pxr/imaging/hd/materialFilteringSceneIndexBase.h>
#include <pxr/imaging/hd/materialNetworkInterface.h>

namespace FVP_NS_DEF {

class PruneTexturesSceneIndex;
typedef PXR_NS::TfRefPtr<PruneTexturesSceneIndex> PruneTexturesSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const PruneTexturesSceneIndex> PruneTexturesSceneIndexConstRefPtr;

class PruneTexturesSceneIndex :
    public PXR_NS::HdMaterialFilteringSceneIndexBase
{
public:
    FVP_API
    static PruneTexturesSceneIndexRefPtr New(
            const PXR_NS::HdSceneIndexBaseRefPtr &inputScene);

    FVP_API
    void MarkTexturesDirty(bool isTextured);
    
    bool _needsTexturesPruned = false;
    
    void _DirtyAllPrims(const PXR_NS::HdDataSourceLocatorSet locators);
    
protected:
    FilteringFnc _GetFilteringFunction() const override;
    
private:
    PruneTexturesSceneIndex(
        PXR_NS::HdSceneIndexBaseRefPtr const &inputSceneIndex);
};

} //end of namespace FVP_NS_DEF

#endif //FVP_PRUNE_TEXTURE_SCENE_INDEX_H
