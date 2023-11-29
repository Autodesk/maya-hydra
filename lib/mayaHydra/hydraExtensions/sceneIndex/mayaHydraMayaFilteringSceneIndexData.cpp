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


//Local headers
#include "mayaHydraMayaFilteringSceneIndexData.h"
#include "mayaHydraLib/mayaUtils.h"

//flow viewport headers
#include <flowViewport/API/fvpFilteringSceneIndexClient.h>

//Maya headers
#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnAttribute.h>
#include <maya/MStringArray.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>

namespace
{
    //Callback when an attribute of this a Maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug& otherPlug, void* data)
    {
        if( ! data){
            return;
        }

        bool isVisible = false;
        if (MayaHydra::IsAMayaVisibilityAttribute(plug, isVisible)){
            PXR_NS::MayaFilteringSceneIndexData* mayaFilteringSceneIndexData    = (PXR_NS::MayaFilteringSceneIndexData*)data;
            mayaFilteringSceneIndexData->updateVisibilityFromDCCNode(isVisible);
        }
    }

    //Callback when a node is added
    void onDagNodeAdded(MObject& obj,  void* data)
    {
        //Since when an object is recreated (such as doing an undo after a delete of the node), the MObject are different but their MObjectHandle::hasCode() are identical so 
        //this is how we identify which object we need to deal with.
        PXR_NS::MayaFilteringSceneIndexData* mayaFilteringSceneIndexData    = (PXR_NS::MayaFilteringSceneIndexData*)data;
        const MObjectHandle objHandle(obj);
        if(mayaFilteringSceneIndexData&& mayaFilteringSceneIndexData->_mObjHandle.isValid() && objHandle.hashCode() == mayaFilteringSceneIndexData->_mObjHandle.hashCode()){
            mayaFilteringSceneIndexData->updateVisibilityFromDCCNode(true);
        }
    }

    //Callback when a node is removed
    void onDagNodeRemoved(MObject& obj,  void* data)
    {
        //Since when an object is recreated (such as doing an undo after a delete of the node), the MObject are different but their MObjectHandle::hasCode() are identical so 
        //this is how we identify which object we need to deal with.
        PXR_NS::MayaFilteringSceneIndexData* mayaFilteringSceneIndexData    = (PXR_NS::MayaFilteringSceneIndexData*)data;
        const MObjectHandle objHandle(obj);
        if(mayaFilteringSceneIndexData&& mayaFilteringSceneIndexData->_mObjHandle.isValid() && objHandle.hashCode() == mayaFilteringSceneIndexData->_mObjHandle.hashCode()){
            mayaFilteringSceneIndexData->updateVisibilityFromDCCNode(false);
        }
    }
}

PXR_NAMESPACE_USING_DIRECTIVE

MayaFilteringSceneIndexData::MayaFilteringSceneIndexData(Fvp::FilteringSceneIndexClient& client)
: PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBase(client)
{
    //If a maya node is present in client.getDccNode(), add callbacks to handle node deleted/undo/redo and hide/unhide
    void* dccNode = client.getDccNode();
    if (dccNode){
        MObject* mObj = reinterpret_cast<MObject*>(dccNode);
        _mObjHandle   = MObjectHandle(*mObj);

        MCallbackId cbId = MNodeMessage::addAttributeChangedCallback(*mObj, attributeChangedCallback, this);
        if (cbId){
            _nodeMessageCallbackIds.append(cbId);
        }

        const MDagPath mayaNodeDagPath  = MDagPath::getAPathTo(*mObj);
                
        //Also monitor parent DAG node to be able to update the scene index if the visibility is modified
        MDagPath parentDagPath  = mayaNodeDagPath;
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
        cbId    = 0;
        cbId = MDGMessage::addNodeAddedCallback(onDagNodeAdded, nodeTypeName, this);
        if (cbId) {
            _dGMessageCallbackIds.append(cbId);
        }

        //Setup node remove callback, filter by node type using nodeTypeName
        cbId    = 0;
        cbId = MDGMessage::addNodeRemovedCallback(onDagNodeRemoved, nodeTypeName, this);
        if (cbId) {
            _dGMessageCallbackIds.append(cbId);
        }
    }
}

MayaFilteringSceneIndexData::~MayaFilteringSceneIndexData()
{
    MNodeMessage::removeCallbacks   (_nodeMessageCallbackIds);
    MMessage::removeCallbacks       (_dGMessageCallbackIds);
}

