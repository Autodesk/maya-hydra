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

/* MayaDataProducerSceneIndexData is storing information about a custom data producer scene index.
*  Since an instance of the MayaDataProducerSceneIndexData class can be shared between multiple viewports, we need ref counting.
*/

//Local headers
#include "mayaHydraMayaDataProducerSceneIndexData.h"
#include "mayaHydraLib/hydraUtils.h"
#include "mayaHydraLib/mayaUtils.h"

//Maya headers
#include <maya/MMatrix.h>
#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MStringArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MPlug.h>

namespace
{
    //Callback when an attribute of a maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* mayaDataProducerSceneIndexData)
    {
        if( ! mayaDataProducerSceneIndexData){
            return;
        }

        //We care only about transform and visibility attributes
        MFnAttribute attr (plug.attribute());
        if (MayaHydra::IsAMayaTransformAttributeName(attr.name())){
            PXR_NS::MayaDataProducerSceneIndexData* MayaDataProducerSceneIndexData = (PXR_NS::MayaDataProducerSceneIndexData*)mayaDataProducerSceneIndexData;
            MayaDataProducerSceneIndexData->UpdateTransformFromMayaNode();
        } else{
            bool isVisible = false;
            if (MayaHydra::IsAMayaVisibilityAttribute(plug, isVisible)){
                PXR_NS::MayaDataProducerSceneIndexData* MayaDataProducerSceneIndexData = (PXR_NS::MayaDataProducerSceneIndexData*)mayaDataProducerSceneIndexData;
                MayaDataProducerSceneIndexData->UpdateVisibilityFromDCCNode(isVisible);
            }
        }
    }

    //Callback when a maya node is added
    void onDagNodeAdded(MObject& obj,  void* data)
    {
        //Since when an object is recreated (when doing an undo after a delete of the node), the MObject from the same node are different but their MObjectHandle::hashCode() are identical so 
        //this is how we identify which object we need to deal with.
        PXR_NS::MayaDataProducerSceneIndexData* mayaDataProducerSceneIndexData    = (PXR_NS::MayaDataProducerSceneIndexData*)data;
        const MObjectHandle objHandle(obj);
        if(mayaDataProducerSceneIndexData && mayaDataProducerSceneIndexData->getObjHandle().isValid() && objHandle.hashCode() == mayaDataProducerSceneIndexData->getObjHandle().hashCode()){
            mayaDataProducerSceneIndexData->UpdateVisibilityFromDCCNode(true);
        }
    }

    //Callback when a maya node is removed
    void onDagNodeRemoved(MObject& obj,  void* data)
    {
        //Since when an object is recreated (when doing an undo after a delete of the node), the MObject from the same node are different but their MObjectHandle::hashCode() are identical so 
        //this is how we identify which object we need to deal with.
        PXR_NS::MayaDataProducerSceneIndexData* mayaDataProducerSceneIndexData    = (PXR_NS::MayaDataProducerSceneIndexData*)data;
        const MObjectHandle objHandle(obj);
        if(mayaDataProducerSceneIndexData && mayaDataProducerSceneIndexData->getObjHandle().isValid() && objHandle.hashCode() == mayaDataProducerSceneIndexData->getObjHandle().hashCode()){
            mayaDataProducerSceneIndexData->UpdateVisibilityFromDCCNode(false);
        }
    }
}

PXR_NAMESPACE_USING_DIRECTIVE

MayaDataProducerSceneIndexData::MayaDataProducerSceneIndexData(const FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params) 
    : FVP_NS_DEF::DataProducerSceneIndexDataBase(params)
{
    //When the user has passed a maya node we are adding callbacks on various changes to be able to act on the data producer scene index prims automatically
    if (_dccNode){
        MObject* mObj = reinterpret_cast<MObject*>(_dccNode);
        _mObjHandle   = MObjectHandle(*mObj);

        _mayaNodeDagPath  = MDagPath::getAPathTo(*mObj);

        _CopyMayaNodeTransform();//Copy it so that the classes created in _CreateSceneIndexChainForDataProducerSceneIndex have the up to date matrix

        //The user provided a DCC node, it's a maya MObject in maya hydra
        _CreateSceneIndexChainForDataProducerSceneIndex();

        MCallbackId cbId = MNodeMessage::addAttributeChangedCallback(*mObj, attributeChangedCallback, this);
        if (cbId){
            _nodeMessageCallbackIds.append(cbId);
        }

        //Also monitor parent DAG node to be able to update the scene index if the parent transform is modified or the visibility changed
        MDagPath parentDagPath  = _mayaNodeDagPath;
        parentDagPath.pop();
        MObject parentObj       = parentDagPath.node();
        cbId    = 0;
        cbId    = MNodeMessage::addAttributeChangedCallback(parentObj, attributeChangedCallback, this);
        if (cbId){
            _nodeMessageCallbackIds.append(cbId);
        }

        //Get node type name to filter by node type for callbacks
        MFnDependencyNode dep(*mObj);
        const MString nodeTypeName = dep.typeName();

        //Setup node added callback, filter by node type using nodeTypeName
        cbId = 0;
        cbId = MDGMessage::addNodeAddedCallback(onDagNodeAdded, nodeTypeName, this);
        if (cbId) {
            _dGMessageCallbackIds.append(cbId);
        }

        //Setup node remove callback, filter by node type using nodeTypeName
        cbId = 0;
        cbId = MDGMessage::addNodeRemovedCallback(onDagNodeRemoved, nodeTypeName, this);
        if (cbId) {
            _dGMessageCallbackIds.append(cbId);
        }
    }
}

MayaDataProducerSceneIndexData::~MayaDataProducerSceneIndexData()
{
    CHECK_MSTATUS(MNodeMessage::removeCallbacks   (_nodeMessageCallbackIds));
    CHECK_MSTATUS(MMessage::removeCallbacks       (_dGMessageCallbackIds));
}

void MayaDataProducerSceneIndexData::UpdateTransformFromMayaNode() 
{ 
    _CopyMayaNodeTransform(); 
    UpdateHydraTransformFromParentPath();
}

void MayaDataProducerSceneIndexData::_CopyMayaNodeTransform() 
{ 
    if (_mayaNodeDagPath.isValid()){
        //Get Maya transform value
        //Convert from Maya matrix to GfMatrix4d
        const MMatrix mayaMat = _mayaNodeDagPath.inclusiveMatrix();
        
        //Copy Maya matrix into _parentMatrix member of this struct
        memcpy(_parentMatrix.GetArray(), mayaMat[0], sizeof(double) * 16);
    }
}

std::string MayaDataProducerSceneIndexData::GetDCCNodeName() const
{
    std::string outNodeName;
    if (_mObjHandle.isValid() && _mObjHandle.isAlive()){
        MFnDependencyNode dep(_mObjHandle.object());
        outNodeName = std::string(dep.name().asChar());
        MAYAHYDRA_NS_DEF::SanitizeNameForSdfPath(outNodeName);
    }

    return outNodeName;
}
