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
#ifndef FVP_DEFAULT_MATERIAL_SCENE_INDEX_H
#define FVP_DEFAULT_MATERIAL_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

class DefaultMaterialSceneIndex;
typedef PXR_NS::TfRefPtr<DefaultMaterialSceneIndex> DefaultMaterialSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const DefaultMaterialSceneIndex> DefaultMaterialSceneIndexConstRefPtr;

class DefaultMaterialSceneIndex :
    public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<DefaultMaterialSceneIndex>
{
public:
    using ParentClass = PXR_NS::HdSingleInputFilteringSceneIndexBase;
    using ParentClass::_GetInputSceneIndex;

    FVP_API
    static DefaultMaterialSceneIndexRefPtr New(
            const PXR_NS::HdSceneIndexBaseRefPtr &inputScene, 
            const PXR_NS::SdfPath& defaultMaterialPath,
            const PXR_NS::SdfPathVector& defaultMaterialExclusionList
            );

    FVP_API
    void Enable(bool enable);
    
   
protected:
    void _MarkMaterialsDirty();
    
    // From HdSceneIndexBase
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override { 
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    // From HdSingleInputFilteringSceneIndexBase
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override{
        if (!_IsObserved())
            return;
        _SendPrimsAdded(entries);
    }

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override
    {
        if (!_IsObserved())
            return;
        _SendPrimsRemoved(entries);
    }
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override
    {
        if (!_IsObserved())
            return;
        _SendPrimsDirtied(entries);
    }

    void _SetDefaultMaterial(PXR_NS::HdSceneIndexPrim& inoutPrim) const;
    bool _ShouldWeApplyTheDefaultMaterial(const PXR_NS::HdSceneIndexPrim& prim)const;

private:
    bool _isEnabled = false;
    const PXR_NS::SdfPath  _defaultMaterialPath;
    PXR_NS::SdfPathVector  _defaultMaterialExclusionList;//These are the materials that should not be affected by the default material, they should be skipped
    
    DefaultMaterialSceneIndex(
        PXR_NS::HdSceneIndexBaseRefPtr const &inputSceneIndex, const PXR_NS::SdfPath& defaultMaterialPath, const PXR_NS::SdfPathVector& defaultMaterialExclusionList);
};

} //end of namespace FVP_NS_DEF

#endif //FVP_DEFAULT_MATERIAL_SCENE_INDEX_H
