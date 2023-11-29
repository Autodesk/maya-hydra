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
#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpParentDataModifierSceneIndex.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>

//The Pixar's namespace needs to be at the highest namespace level for TF_DECLARE_WEAK_AND_REF_PTRS to work.
PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

class DataProducerSceneIndexDataBase;//Predeclaration
TF_DECLARE_WEAK_AND_REF_PTRS(DataProducerSceneIndexDataBase);//Be able to use Ref counting pointers on DataProducerSceneIndexDataBase

/** DataProducerSceneIndexDataBase is storing information about a custom data producer scene index.
*   Since an instance of the DataProducerSceneIndexDataBase class can be shared between multiple viewports in our records, we need ref counting.
*/
 class FVP_API DataProducerSceneIndexDataBase : public TfRefBase, public TfWeakBase
{
public:
    
    ///Structure passed as a parameter to create an instance of DataProducerSceneIndexDataBase
    struct CreationParameters
    {
        CreationParameters( const HdSceneIndexBaseRefPtr&   customDataProducerSceneIndex,
                            const std::string&              rendererNames,
                            const SdfPath&                  customDataProducerSceneIndexRootPathForInsertion,
                            void*                           dccNode) :
            _customDataProducerSceneIndex(customDataProducerSceneIndex), _rendererNames(rendererNames),
            _customDataProducerSceneIndexRootPathForInsertion(customDataProducerSceneIndexRootPathForInsertion),_dccNode(dccNode)
        {}

        //See below for an explanation of these parameters
        const HdSceneIndexBaseRefPtr&   _customDataProducerSceneIndex;
        const std::string&              _rendererNames;
        const SdfPath&                  _customDataProducerSceneIndexRootPathForInsertion;
        void*                           _dccNode;
    };

    ~DataProducerSceneIndexDataBase() override = default;
    
    virtual void AddParentPrimToSceneIndex();
    virtual void RemoveParentPrimFromSceneIndex();

    const HdSceneIndexBaseRefPtr&   GetDataProducerSceneIndex() const {return _dataProducerSceneIndex;}
    const HdSceneIndexBaseRefPtr&   GetDataProducerLastSceneIndexChain() const {return _lastSceneIndexChain;}
    const SdfPath&                  GetCustomDataProducerSceneIndexRootPathForInsertion()const{return _customDataProducerSceneIndexRootPathForInsertion;}
    const std::string&              GetRendererNames() const {return _rendererNames;}

    /// Provide the node name from the DCC to be overriden in a DCC specific subclass
    virtual std::string GetDCCNodeName() const {return "";}

    void UpdateVisibilityFromDCCNode(bool isVisible);
    void UpdateHydraTransformFromParentPath();

protected:

    void _CreateSceneIndexChainForDataProducerSceneIndex();
    DataProducerSceneIndexDataBase(const CreationParameters& params);

    /// data producer scene index
    HdSceneIndexBaseRefPtr              _dataProducerSceneIndex = nullptr;
    /// data producer scene index rootPath for insertion (used in HdRenderIndex::InsertSceneIndex)
    SdfPath                             _customDataProducerSceneIndexRootPathForInsertion;
    /// Are the Hydra renderer(s) to which this scene index should be applied (e.g : "GL, Arnold") or DataProducerSceneIndexInterface::allViewports to apply to all viewports
    std::string                         _rendererNames;
    /// Is the DCC node so a MObject* for Maya
    void*                               _dccNode;

    //The following members are optional and only used when a dccNode was passed in the constructor
    /** Is a filtering scene index that modifies the parent prim from the retained scene index to update the transform/visibility when it is updated in the DCC. 
    It is used only when a dccNode was passed.*/
    ParentDataModifierSceneIndexRefPtr  _parentDataModifierSceneIndex = nullptr;
    /// ParentPath prim used to be the parent of all prims from _dataProducerSceneIndex, used only when a dccNode was passed
    SdfPath                             _parentPath;
    /// Is the last scene index of the scene index chain when a dccNode was passed.
    HdSceneIndexBaseRefPtr              _lastSceneIndexChain = nullptr;
    /// Is the retained scene index holding the parent prim for _dataProducerSceneIndex. It is used only when a dccNode was passed.
    HdRetainedSceneIndexRefPtr          _retainedSceneIndex = nullptr;
    
    /// Is the world matrix of the parent prim in the retained scene index. It is used only when a dccNode was passed.
    GfMatrix4d                          _parentMatrix;
    /// Is the visibility state of the parent prim in the retained scene index. It is used only when a dccNode was passed.
    bool                                _visible = false;
};

}//End of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_DATA_PRODUCER_SCENE_INDEX_DATA_BASE_H

