//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

//Flow viewport headers
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>
#include <flowViewport/API/fvpInformationInterface.h>
#include <flowViewport/API/fvpVersionInterface.h>
#include <flowViewport/API/samples/fvpInformationClientExample.h>
#include <flowViewport/API/samples/fvpDataProducerSceneIndexExample.h>

//Maya hydra headers
#include <mayaHydraLib/mayaUtils.h>

//Maya headers
#include <maya/MPxLocatorNode.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnTransform.h>
#include <maya/MMatrix.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MDagPath.h>
#include <maya/MNodeMessage.h>
#include <maya/MSceneMessage.h>


//We use a locator node to deal with creating and filtering hydra primitives as an example.
//But you could use another kind of maya plugin.

/*To create an instance of this node in maya, please use the following MEL command :
createNode("FlowViewportAPIMayaLocator")
*/

PXR_NAMESPACE_USING_DIRECTIVE

///Maya Locator node subclass to create filtering and data producer scene indices example, to be used with the flow viewport API.
class FlowViewportAPIMayaLocator : public MPxLocatorNode
{
public:
    ~FlowViewportAPIMayaLocator() override;

    MStatus   		compute( const MPlug& plug, MDataBlock& data ) override;

    bool            isBounded() const override;
    MBoundingBox    boundingBox() const override;

    MStatus preEvaluation(const MDGContext& context, const MEvaluationNode& evaluationNode) override;
    void    getCacheSetup(const MEvaluationNode& evalNode, MNodeCacheDisablingInfo& disablingInfo, MNodeCacheSetupInfo& cacheSetupInfo, MObjectArray& monitoredAttributes) const override;

    void SetCubeGridParametersFromAttributes();
    void SetupFlowViewportInterfaces();

    //static members
    static  void*       creator();
    static  MStatus     initialize();
    static	MTypeId		id;
    static	MString		nodeClassification;

    //Maya node attributes that helps creating a 3D grid of Hydra cube mesh primitives to demonstrate how to inject some primitives in a Hydra viewport
    static MObject mNumCubeLevelsX;
    static MObject mNumCubeLevelsY;
    static MObject mNumCubeLevelsZ;
    static MObject mCubeHalfSize;
    static MObject mCubeInitalTransform;
    static MObject mCubeColor;
    static MObject mCubeOpacity;
    static MObject mCubesUseInstancing;
    static MObject mCubesDeltaTrans;
    static MObject mDummyInput;//Dummy input to trigger a call to compute
    static MObject mDummyOutput;//Dummy output to trigger a call to compute
    
    ///3D Grid of cube mesh primitives creation parameters for the data producer scene index
    Fvp::DataProducerSceneIndexExample::CubeGridCreationParams  _cubeGridParams;
    ///_hydraViewportDataProducerSceneIndexExample is what will inject the 3D grid of Hydra cube mesh primitives into the viewport
    Fvp::DataProducerSceneIndexExample                          _hydraViewportDataProducerSceneIndexExample;

protected:
    /// _hydraViewportInformationClient is the viewport information example for a Hydra viewport.
    std::shared_ptr<Fvp::InformationClientExample>          _hydraViewportInformationClient;
    ///To be used in hydra viewport API to pass the Maya node's MObject for setting callbacks for filtering and data producer scene indices
    MObject                                                 _thisMObject; 
    ///To check if the MObject of this node has changed
    MObject                                                 _oldMObject; 
    ///To hold the attributeChangedCallback Id to be able to react when the 3D grid creation parameters attributes from this node change.
    MCallbackId                                             _cbAttributeChangedId = 0;
    ///To hold the afterOpenCallback Id to be able to react when a File Open has happened.
    MCallbackId                                             _cbAfterOpenId = 0;
    ///To hold the transformAttributeChangedCallback Id to be able to react when the transform matrix has changed.
    MCallbackId                                             _cbParentAttributeChangedId;
    
private:
    // Private Constructor
    FlowViewportAPIMayaLocator();
    /// Init flag to be able to do things only once for this node
    bool _init = false;
};

namespace
{
    //Get node inverse transform matrix (needed by instancing in a dataProducerSceneIndex)
    MStatus GetNodeInverseTransform(MObject& mObj, GfMatrix4d& outInvTransform) 
    {
        outInvTransform = outInvTransform.SetIdentity();

        MStatus stat;

        //Try directly if the mObj is a transform node
        MObject nodeToGetTransform = mObj;
        if ( ! mObj.hasFn(MFn::kTransform)){
            //Try with the parent dag path from that node
            MDagPath dagPath = MDagPath::getAPathTo(mObj, &stat);
            CHECK_MSTATUS(stat);
            dagPath.pop();
            nodeToGetTransform = dagPath.node();
            if ( ! nodeToGetTransform.hasFn(MFn::kTransform)){
                return MStatus::kInvalidParameter;//Stopping here
            }
        }
        
        MFnTransform transform(nodeToGetTransform, &stat);
        CHECK_MSTATUS(stat);
        if (MStatus::kSuccess != stat){
            return MStatus::kFailure;
        }

        MMatrix transformMatrix = transform.transformationMatrix(&stat);
        CHECK_MSTATUS(stat);
        if (MStatus::kSuccess != stat){
            return MStatus::kFailure;
        }

        MMatrix invTransform = transformMatrix.inverse();
        memcpy(outInvTransform.GetArray(), invTransform[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d

        return MStatus::kSuccess;
    }

    //Callback when an attribute of this Maya node changes
    void transformAttributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* dataProducerSceneIndexData)
    {
        //Dealing with the transform only
        MFnAttribute attr (plug.attribute());
        if (MayaHydra::IsAMayaTransformAttributeName(attr.name())){
            
            if (! dataProducerSceneIndexData){
                return;
            }
            FlowViewportAPIMayaLocator* flowViewportAPIMayaLocator = reinterpret_cast<FlowViewportAPIMayaLocator*>(dataProducerSceneIndexData);

            GfMatrix4d nodeInvTransform;
            MObject mObj = flowViewportAPIMayaLocator->thisMObject();
            MStatus stat = GetNodeInverseTransform(mObj, nodeInvTransform);
            CHECK_MSTATUS(stat);
            if (MStatus::kSuccess == stat){
                flowViewportAPIMayaLocator->_hydraViewportDataProducerSceneIndexExample.setContainerNodeInverseTransform(nodeInvTransform);
            }
        }
    }

    //Callback when an attribute of this Maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* dataProducerSceneIndexData)
    {
        if (! dataProducerSceneIndexData){
            return;
        }

        FlowViewportAPIMayaLocator* flowViewportAPIMayaLocator = reinterpret_cast<FlowViewportAPIMayaLocator*>(dataProducerSceneIndexData);

        MPlug parentPlug = plug.parent();
        if (plug == flowViewportAPIMayaLocator->mDummyInput || plug == flowViewportAPIMayaLocator->mDummyOutput){
            return; //These attributes are not related to the cubes grid
        }

        if (plug == flowViewportAPIMayaLocator->mNumCubeLevelsX){
            flowViewportAPIMayaLocator->_cubeGridParams._numLevelsX = plug.asInt();
        }else
        if (plug == flowViewportAPIMayaLocator->mNumCubeLevelsY){
            flowViewportAPIMayaLocator->_cubeGridParams._numLevelsY = plug.asInt();
        }else
        if (plug == flowViewportAPIMayaLocator->mNumCubeLevelsZ){
            flowViewportAPIMayaLocator->_cubeGridParams._numLevelsZ = plug.asInt();
        }else
        if (plug == flowViewportAPIMayaLocator->mCubeHalfSize){
            flowViewportAPIMayaLocator->_cubeGridParams._halfSize = plug.asDouble();
        }else
        if (plug == flowViewportAPIMayaLocator->mCubeInitalTransform){
            auto dataHandle                                                 = plug.asMDataHandle();
            const MMatrix   mat                                             = dataHandle.asMatrix();
            memcpy(flowViewportAPIMayaLocator->_cubeGridParams._initalTransform.GetArray(), mat[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d
        }else
        if (parentPlug == flowViewportAPIMayaLocator->mCubeColor){
            auto dataHandle                                                 = parentPlug.asMDataHandle();//Using parent plug as this is composed of 3 doubles
            const double3& color                                            = dataHandle.asDouble3();
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[0]    = color[0];//Implicit conversion from double to float
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[1]    = color[1];
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[2]    = color[2];
        }else
        if (plug == flowViewportAPIMayaLocator->mCubeColor){
            auto dataHandle                                                 = plug.asMDataHandle();
            const double3& color                                            = dataHandle.asDouble3();
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[0]    = color[0];//Implicit conversion from double to float
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[1]    = color[1];
            flowViewportAPIMayaLocator->_cubeGridParams._color.data()[2]    = color[2];
        }else
        if (plug == flowViewportAPIMayaLocator->mCubeOpacity){
            flowViewportAPIMayaLocator->_cubeGridParams._opacity = plug.asDouble();
        }else
        if (plug == flowViewportAPIMayaLocator->mCubesUseInstancing){
            flowViewportAPIMayaLocator->_cubeGridParams._useInstancing = plug.asBool();
        }else
        if (parentPlug == flowViewportAPIMayaLocator->mCubesDeltaTrans){
            auto dataHandle                                                     = parentPlug.asMDataHandle();//Using parent plug as this is composed of 3 doubles
            const double3& deltaTrans                                           = dataHandle.asDouble3();
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[0]   = deltaTrans[0];//Implicit conversion from double to float
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[1]   = deltaTrans[1];
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[2]   = deltaTrans[2];
        }else
        if (plug == flowViewportAPIMayaLocator->mCubesDeltaTrans){
            auto dataHandle                                                     = plug.asMDataHandle();
            const double3& deltaTrans                                           = dataHandle.asDouble3();
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[0]   = deltaTrans[0];//Implicit conversion from double to float
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[1]   = deltaTrans[1];
            flowViewportAPIMayaLocator->_cubeGridParams._deltaTrans.data()[2]   = deltaTrans[2];
        }else{
            return; //Not a grid cube attribute
        }

        
        flowViewportAPIMayaLocator->_hydraViewportDataProducerSceneIndexExample.setCubeGridParams(flowViewportAPIMayaLocator->_cubeGridParams);
    }

    template <class T> MStatus GetAttributeValue(T& outVal, const MObject& node, const MObject& attr)
    {
        MPlug plug(node, attr);
        return plug.getValue(outVal);
    }

    void GetMatrixAttributeValue(MMatrix& outVal, const MObject& node, const MObject& attr)
    {
        MPlug plug(node, attr);
        MObject oMatrix;
        plug.getValue(oMatrix);

        MFnMatrixData fnData(oMatrix);
	    outVal = fnData.matrix();
    }

    void GetDouble3AttributeValue(double3& outVal, const MObject& node, const MObject& attr)
    {
        MPlug plug(node, attr);
        MObject oDouble3;
        plug.getValue(oDouble3);

        MFnNumericData fnData(oDouble3);
        fnData.getData( outVal[0], outVal[1], outVal[2] );
    }

    //Callback after a File Open
    void afterOpenCallback (void *clientData) 
    {
        if (! clientData){
            return;
        }

        FlowViewportAPIMayaLocator* flowViewportAPIMayaLocator = reinterpret_cast<FlowViewportAPIMayaLocator*>(clientData);
        flowViewportAPIMayaLocator->SetCubeGridParametersFromAttributes();
        flowViewportAPIMayaLocator->SetupFlowViewportInterfaces();
    }

}//end of anonymous namespace

//Initialization of static members
MTypeId FlowViewportAPIMayaLocator::id( 0x90517 );
MString	FlowViewportAPIMayaLocator::nodeClassification("hydraAPIExample/geometry/FlowViewportAPIMayaLocator");

MObject FlowViewportAPIMayaLocator::mNumCubeLevelsX;
MObject FlowViewportAPIMayaLocator::mNumCubeLevelsY;
MObject FlowViewportAPIMayaLocator::mNumCubeLevelsZ;
MObject FlowViewportAPIMayaLocator::mCubeHalfSize;
MObject FlowViewportAPIMayaLocator::mCubeInitalTransform;
MObject FlowViewportAPIMayaLocator::mCubeColor;
MObject FlowViewportAPIMayaLocator::mCubeOpacity;
MObject FlowViewportAPIMayaLocator::mCubesUseInstancing;
MObject FlowViewportAPIMayaLocator::mCubesDeltaTrans;
MObject FlowViewportAPIMayaLocator::mDummyInput;
MObject FlowViewportAPIMayaLocator::mDummyOutput;

//Macro to create input attribute for the maya node
#define MAKE_INPUT(attr)	\
    CHECK_MSTATUS(attr.setKeyable(true) );		\
    CHECK_MSTATUS(attr.setStorable(true) );		\
    CHECK_MSTATUS(attr.setReadable(true) );		\
    CHECK_MSTATUS(attr.setWritable(true) );		\
	CHECK_MSTATUS(attr.setAffectsAppearance(true) );

//Macro to create output attribute for the maya node
#define MAKE_OUTPUT(attr)								\
CHECK_MSTATUS ( attr.setKeyable(false) ); \
CHECK_MSTATUS ( attr.setStorable(false) );	\
CHECK_MSTATUS ( attr.setReadable(true) ); \
CHECK_MSTATUS ( attr.setWritable(false) );


FlowViewportAPIMayaLocator::FlowViewportAPIMayaLocator()
{
    //Get the flow viewport API hydra interfaces
    int majorVersion = 0;
    int minorVersion = 0;
    int patchLevel   = 0;

    //Get the version of the flow viewport APIinterface
    Fvp::VersionInterface::Get().GetVersion(majorVersion, minorVersion, patchLevel);
    
    //data producer scene index interface
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();

    //Store the interface pointer into our client for later
    _hydraViewportDataProducerSceneIndexExample.setHydraInterface(&dataProducerSceneIndexInterface);

    //Viewport Information interface
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();

    _hydraViewportInformationClient = std::make_shared<Fvp::InformationClientExample>();

    //Register this viewport information client, so it gets called when Hydra viewports scene indices are created/removed
    informationInterface.RegisterInformationClient(_hydraViewportInformationClient);

    //Add a callback after a load scene
    _cbAfterOpenId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, afterOpenCallback, ((void*)this)) ;
}

//This is called only when our node is destroyed and the undo queue flushed.
FlowViewportAPIMayaLocator::~FlowViewportAPIMayaLocator() 
{ 
    //Remove the callbacks
    if (_cbAttributeChangedId){
        CHECK_MSTATUS(MMessage::removeCallback(_cbAttributeChangedId));
        _cbAttributeChangedId = 0;
    }

    if (_cbAfterOpenId){
        CHECK_MSTATUS(MSceneMessage::removeCallback(_cbAfterOpenId));
        _cbAfterOpenId = 0;
    }

    if (_cbParentAttributeChangedId){
        CHECK_MSTATUS(MSceneMessage::removeCallback(_cbParentAttributeChangedId));
        _cbParentAttributeChangedId = 0;
    }

    //The DataProducerSceneIndexExample in its destructor removes itself by calling DataProducerSceneIndexExample::RemoveDataProducerSceneIndex()
     
    //Unregister viewport information client
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    informationInterface.UnregisterInformationClient(_hydraViewportInformationClient);
}

void FlowViewportAPIMayaLocator::SetCubeGridParametersFromAttributes()
{
    const MObject mObj = thisMObject();
    if (mObj.isNull()){
        return;
    }

    GetAttributeValue(_cubeGridParams._numLevelsX, mObj, FlowViewportAPIMayaLocator::mNumCubeLevelsX);
    GetAttributeValue(_cubeGridParams._numLevelsY, mObj, FlowViewportAPIMayaLocator::mNumCubeLevelsY);
    GetAttributeValue(_cubeGridParams._numLevelsZ, mObj, FlowViewportAPIMayaLocator::mNumCubeLevelsZ);

    GetAttributeValue(_cubeGridParams._halfSize, mObj, FlowViewportAPIMayaLocator::mCubeHalfSize);
    
    MMatrix mat;
    GetMatrixAttributeValue(mat, mObj, FlowViewportAPIMayaLocator::mCubeInitalTransform);
    memcpy(_cubeGridParams._initalTransform.GetArray(), mat[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d

    double3 color;
    GetDouble3AttributeValue(color, mObj, FlowViewportAPIMayaLocator::mCubeColor);
    _cubeGridParams._color.data()[0]        = color[0];//Implicit conversion from double to float
    _cubeGridParams._color.data()[1]        = color[1];
    _cubeGridParams._color.data()[2]        = color[2];

    GetAttributeValue(_cubeGridParams._opacity, mObj, FlowViewportAPIMayaLocator::mCubeOpacity);
    GetAttributeValue(_cubeGridParams._useInstancing, mObj, FlowViewportAPIMayaLocator::mCubesUseInstancing);

    double3 deltaTrans;
    GetDouble3AttributeValue(deltaTrans, mObj, FlowViewportAPIMayaLocator::mCubesDeltaTrans);
    _cubeGridParams._deltaTrans.data()[0]   = deltaTrans[0];//Implicit conversion from double to float
    _cubeGridParams._deltaTrans.data()[1]   = deltaTrans[1];
    _cubeGridParams._deltaTrans.data()[2]   = deltaTrans[2];

    _hydraViewportDataProducerSceneIndexExample.setCubeGridParams(_cubeGridParams);
}

void FlowViewportAPIMayaLocator::SetupFlowViewportInterfaces()
{
    if (_thisMObject.isNull()){
        MObject currentMObj = thisMObject();
        if (_oldMObject.isNull() || _oldMObject != currentMObj){
            _thisMObject = currentMObj;     
            _oldMObject  = _thisMObject;
        }

        if(_thisMObject.isNull()){
            return;
        }
    }

    //Store the MObject* of the maya node in various classes
    //Set the maya node as a parent for this data producer scene index so that when the node is hidden/deleted/moved it gets applied to the prims produced
    GfMatrix4d nodeInvTransform;
    GetNodeInverseTransform(_thisMObject, nodeInvTransform);
    _hydraViewportDataProducerSceneIndexExample.setContainerNode(&_thisMObject);
    _hydraViewportDataProducerSceneIndexExample.setContainerNodeInverseTransform(nodeInvTransform);
    _hydraViewportDataProducerSceneIndexExample.addDataProducerSceneIndex();
}

MStatus FlowViewportAPIMayaLocator::compute( const MPlug& plug, MDataBlock& dataBlock)
{
    //Do it only once per call to this function per node
    
    if( ! _init){
        SetCubeGridParametersFromAttributes();

        MObject currentMObj = thisMObject();
        //Add the callback when an attribute of this node changes
        _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(currentMObj, attributeChangedCallback, ((void*)this));

        //Also monitor parent DAG node to be able to update the scene index if the parent transform is modified
        MStatus stat;
        MDagPath parentDagPath = MDagPath::getAPathTo(currentMObj, &stat);
        CHECK_MSTATUS(stat);
        parentDagPath.pop();
        MObject parentObj = parentDagPath.node();
        _cbParentAttributeChangedId = MNodeMessage::addAttributeChangedCallback(parentObj, transformAttributeChangedCallback, this);
        
        _init                               = true;
    }

    //The MObject can change if the node gets deleted and deletion being undone, so always update it in our records
    MObject currentMObj = thisMObject();
    if (_oldMObject.isNull() || _oldMObject != currentMObj){
        
        _thisMObject = currentMObj;     
        _oldMObject  = _thisMObject;

        SetupFlowViewportInterfaces();
    }
        
    
    return MS::kSuccess;
}

bool FlowViewportAPIMayaLocator::isBounded() const
{
    return true;
}

//We return as a bounding box the bounding box of the data producer hydra data
MBoundingBox FlowViewportAPIMayaLocator::boundingBox() const
{
    float corner1X, corner1Y, corner1Z, corner2X, corner2Y, corner2Z;
    _hydraViewportDataProducerSceneIndexExample.getPrimsBoundingBox(corner1X, corner1Y, corner1Z, corner2X, corner2Y, corner2Z);
    return MBoundingBox( {corner1X, corner1Y, corner1Z}, {corner2X, corner2Y, corner2Z});
}

// Called before this node is evaluated by Evaluation Manager
MStatus FlowViewportAPIMayaLocator::preEvaluation(
    const MDGContext& context,
    const MEvaluationNode& evaluationNode)
{
    return MStatus::kSuccess;
}

void FlowViewportAPIMayaLocator::getCacheSetup(const MEvaluationNode& evalNode, MNodeCacheDisablingInfo& disablingInfo, MNodeCacheSetupInfo& cacheSetupInfo, MObjectArray& monitoredAttributes) const
{
    MPxLocatorNode::getCacheSetup(evalNode, disablingInfo, cacheSetupInfo, monitoredAttributes);
    assert(!disablingInfo.getCacheDisabled());
    cacheSetupInfo.setPreference(MNodeCacheSetupInfo::kWantToCacheByDefault, true);
}

void* FlowViewportAPIMayaLocator::creator()
{
    return new FlowViewportAPIMayaLocator;
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Plugin Registration
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
MStatus FlowViewportAPIMayaLocator::initialize()
{
    MStatus status;

    //Create input attributes for the 3D grid of Hydra cube mesh primitives creation for the data producer scene index
    MFnNumericAttribute nAttr;
    MFnMatrixAttribute  mAttr;
    
    mNumCubeLevelsX  = nAttr.create("numCubesX", "nX", MFnNumericData::kInt, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(10) );

    mNumCubeLevelsY  = nAttr.create("numCubesY", "nY", MFnNumericData::kInt, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(10) );
    
    mNumCubeLevelsZ  = nAttr.create("numCubesZ", "nZ", MFnNumericData::kInt, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(1) );

    mCubeHalfSize = nAttr.create("cubeHalfSize", "cHS", MFnNumericData::kDouble, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(2.0) );
    
    mCubeInitalTransform = mAttr.create("cubeInitalTransform", "cIT", MFnMatrixAttribute::kDouble, &status);
    MAKE_INPUT(mAttr);
    
    mCubeColor = nAttr.create("cubeColor", "cC", MFnNumericData::k3Double, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(0.0, 1.0, 0.0) );
    
    mCubeOpacity = nAttr.create("cubeOpacity", "cO", MFnNumericData::kDouble, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(0.8) );

    mCubesUseInstancing = nAttr.create("cubesUseInstancing", "cUI", MFnNumericData::kBoolean, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(false) );

    mCubesDeltaTrans = nAttr.create("cubesDeltaTrans", "cDT", MFnNumericData::k3Double, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(5.0, 5.0, 5.0) );

    //Create dummy output attribute to trigger a call to the compute function on demand. as it's in the compute fonction that we add our scene indices
    mDummyInput = nAttr.create("dummyInput", "dI", MFnNumericData::kInt, 1.0, &status);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(1) );

    //Create dummy output attribute to trigger a call to the compute function on demand. as it's in the compute fonction that we add our scene indices
    mDummyOutput = nAttr.create("dummyOutput", "dO", MFnNumericData::kInt, 1.0, &status);
    MAKE_OUTPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(1) );
    
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsX));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsY));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsZ));
    CHECK_MSTATUS ( addAttribute(mCubeHalfSize));
    CHECK_MSTATUS ( addAttribute(mCubeInitalTransform));
    CHECK_MSTATUS ( addAttribute(mCubeColor));
    CHECK_MSTATUS ( addAttribute(mCubeOpacity));
    CHECK_MSTATUS ( addAttribute(mCubesUseInstancing));
    CHECK_MSTATUS ( addAttribute(mCubesDeltaTrans));
    CHECK_MSTATUS ( addAttribute(mDummyInput));
    CHECK_MSTATUS ( addAttribute(mDummyOutput));

    CHECK_MSTATUS ( attributeAffects(mDummyInput, mDummyOutput));

    
    return status;
}

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    static const char * pluginVersion = "1.0";
    MFnPlugin plugin( obj, PLUGIN_COMPANY, pluginVersion, "Any");

    status = plugin.registerNode(
                "FlowViewportAPIMayaLocator",
                FlowViewportAPIMayaLocator::id,
                &FlowViewportAPIMayaLocator::creator,
                &FlowViewportAPIMayaLocator::initialize,
                MPxNode::kLocatorNode,
                &FlowViewportAPIMayaLocator::nodeClassification);
    if (!status) {
        status.perror("registerNode");
        return status;
    }

    return status;
}

MStatus uninitializePlugin( MObject obj)
{
    MStatus   status;
    MFnPlugin plugin( obj );

    status = plugin.deregisterNode( FlowViewportAPIMayaLocator::id );
    if (!status) {
        status.perror("deregisterNode");
        return status;
    }
    return status;
}
