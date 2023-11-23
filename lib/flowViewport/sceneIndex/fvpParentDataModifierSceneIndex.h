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
#ifndef FLOW_VIEWPORT_SCENEINDEX_PARENT_DATA_MODIFIER_SCENE_INDEX_H
#define FLOW_VIEWPORT_SCENEINDEX_PARENT_DATA_MODIFIER_SCENE_INDEX_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>

//The Pixar's namespace needs to be at the highest namespace level for TF_DECLARE_WEAK_AND_REF_PTRS to work.
PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

//We use a filtering scene index to update the data from a HdRetainedSceneIndex where we inserted a parent prim to be the parent of all prims from a data dataProducer scene index
// which is hosted in a DCC node.
 
class ParentDataModifierSceneIndex;
TF_DECLARE_WEAK_AND_REF_PTRS(ParentDataModifierSceneIndex);

class ParentDataModifierSceneIndex : public HdSingleInputFilteringSceneIndexBase
{
public:
    using ParentClass = HdSingleInputFilteringSceneIndexBase;

    static ParentDataModifierSceneIndexRefPtr New(const HdSceneIndexBaseRefPtr& _inputSceneIndex){
        return TfCreateRefPtr(new ParentDataModifierSceneIndex(_inputSceneIndex));
    }

    void SetParentPath              (const SdfPath& parentPath)         {_parentPath        = parentPath;}
    void SetParentTransformMatrix   (const GfMatrix4d& transformMatrix) {_transformMatrix   = transformMatrix;}
    void SetParentVisibility        (bool visible)                      {_visible           = visible;}

    // From HdSceneIndexBase
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;
    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override { //defined in this header file as we do no modification to it
        if (_GetInputSceneIndex()){
            return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
        }

        return {};
    }
    
    ~ParentDataModifierSceneIndex() override  = default;

protected:
    void _PrimsAdded(
        const HdSceneIndexBase&                       sender,
        const HdSceneIndexObserver::AddedPrimEntries& entries) override final
    {
        _SendPrimsAdded(entries);
    }

    void _PrimsRemoved(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::RemovedPrimEntries& entries) override final
    {
        _SendPrimsRemoved(entries);
    }

    void _PrimsDirtied(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::DirtiedPrimEntries& entries) override final
        {
            _SendPrimsDirtied(entries);
        }

    SdfPath     _parentPath;
    GfMatrix4d  _transformMatrix;
    bool        _visible = true;
private:
    ParentDataModifierSceneIndex(const HdSceneIndexBaseRefPtr& _inputSceneIndex) : ParentClass(_inputSceneIndex){}
};

} //End of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_SCENEINDEX_PARENT_DATA_MODIFIER_SCENE_INDEX_H

