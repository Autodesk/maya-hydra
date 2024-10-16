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
#ifndef FLOW_VIEWPORT_SCENE_INDEX_FLOW_VIEWPORT_LIGHTS_MANAGEMENT_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENE_INDEX_FLOW_VIEWPORT_LIGHTS_MANAGEMENT_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

class LightsManagementSceneIndex;
typedef PXR_NS::TfRefPtr<LightsManagementSceneIndex> LightsManagementSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const LightsManagementSceneIndex> LightsManagementSceneIndexConstRefPtr;

/// \class LightsManagementSceneIndex
///
/// This is a filtering scene index that manages lights primitives
///
class LightsManagementSceneIndex : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<LightsManagementSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static LightsManagementSceneIndexRefPtr New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const PathInterface& pathInterface, const PXR_NS::SdfPath& defaultLightPath){
        return PXR_NS::TfCreateRefPtr(new LightsManagementSceneIndex(inputSceneIndex, pathInterface, defaultLightPath));
    }

    // From HdSceneIndexBase
    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;
    
    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override{
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }
    
    FVP_API
    ~LightsManagementSceneIndex() override = default;

    enum class LightingMode{
        kNoLighting,
        kSceneLighting,//All lights
        kDefaultLighting,
        kSelectedLightsOnly,
    };

    FVP_API
    void SetLightingMode(LightingMode lightingMode);

    FVP_API
    LightingMode GetLightingMode()const {return _lightingMode;}
    
protected:
    
    LightsManagementSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex, const PathInterface& pathInterface, const PXR_NS::SdfPath& defaultLightPath);

    //From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override{
        if (!_IsObserved())return;
        _SendPrimsAdded(entries);
    }
    void _PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries)override{
        if (!_IsObserved())return;
        _SendPrimsDirtied(entries);
    }

    void _DirtyAllLightsPrims();
    bool _IsDefaultLight(const PXR_NS::SdfPath& primPath)const;

    LightingMode _lightingMode = LightingMode::kSceneLighting;
    PXR_NS::SdfPath _defaultLightPath;
    const PathInterface& _pathInterface;
};

}//end of namespace FVP_NS_DEF

#endif //FLOW_VIEWPORT_SCENE_INDEX_FLOW_VIEWPORT_LIGHTS_MANAGEMENT_SCENE_INDEX_H
