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
#include <flowViewport/API/fvpFilteringSceneIndexInterface.h>
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>
#include <flowViewport/API/fvpInformationInterface.h>
#include <flowViewport/API/fvpVersionInterface.h>
#include <flowViewport/API/samples/fvpInformationClientExample.h>
#include <flowViewport/API/samples/fvpDataProducerSceneIndexExample.h>
#include <flowViewport/API/samples/fvpFilteringSceneIndexClientExample.h>

//Maya hydra headers
#include <mayaHydraLib/mayaUtils.h>

//Maya headers
#include <maya/MPxSurfaceShape.h>
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
#include <maya/MObjectHandle.h>
#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MModelMessage.h>

//We use a surface shape node to deal with creating and filtering hydra primitives as an example.
//But you could use another kind of maya plugin.

/*To create an instance of this node in maya, please use the following MEL command :
createNode("MhFlowViewportAPILocator")
*/

namespace {
void nodeAddedToModel(MObject& node, void* clientData);
void nodeRemovedFromModel(MObject& node, void* clientData);
}

PXR_NAMESPACE_USING_DIRECTIVE

///Maya SurfaceShape node subclass to create filtering and data producer scene indices example, to be used with the flow viewport API.
class MhFlowViewportAPILocator : public MPxSurfaceShape
{
public:
    ~MhFlowViewportAPILocator() override;

    void    postConstructor() override;

    MStatus   		compute( const MPlug& plug, MDataBlock& data ) override;

    bool            isBounded() const override;
    MBoundingBox    boundingBox() const override;

    MStatus preEvaluation(const MDGContext& context, const MEvaluationNode& evaluationNode) override;
    void    getCacheSetup(const MEvaluationNode& evalNode, MNodeCacheDisablingInfo& disablingInfo, MNodeCacheSetupInfo& cacheSetupInfo, MObjectArray& monitoredAttributes) const override;

    void setCubeGridParametersFromAttributes();

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
    
    ///3D Grid of cube mesh primitives creation parameters for the data producer scene index
    Fvp::DataProducerSceneIndexExample::CubeGridCreationParams  _cubeGridParams;
    ///_hydraViewportDataProducerSceneIndexExample is what will inject the 3D grid of Hydra cube mesh primitives into the viewport
    Fvp::DataProducerSceneIndexExample                          _hydraViewportDataProducerSceneIndexExample;

    // Callback when the footprint node is added to the model (create /
    // undo-delete)
    void addedToModelCb();
    // Callback when the footprint node is removed from model (delete)
    void removedFromModelCb();

protected:
    /// _hydraViewportFilteringSceneIndexClientExample is the filtering scene index example for a Hydra viewport scene index.
    std::shared_ptr<Fvp::FilteringSceneIndexClientExample>  _hydraViewportFilteringSceneIndexClientExample;
    /// _hydraViewportInformationClient is the viewport information example for a Hydra viewport.
    std::shared_ptr<Fvp::InformationClientExample>          _hydraViewportInformationClient;
    ///To be used in hydra viewport API to pass the Maya node's MObject for setting callbacks for filtering and data producer scene indices
    MObjectHandle                                           _thisMObject; 
    ///To hold the attributeChangedCallback Id to be able to react when the 3D grid creation parameters attributes from this node change.
    MCallbackId                                             _cbAttributeChangedId {0};
    ///To hold the afterOpenCallback Id to be able to react when a File Open has happened.
    MCallbackId                                             _cbAfterOpenId {0};
    /// to hold the nodeAddedToModel callback id
    MCallbackId                                             _nodeAddedToModelCbId{0};
    /// to hold the nodeRemovedFromModel callback id
    MCallbackId                                             _nodeRemovedFromModelCbId{0};

private:
    /// Private Constructor
    MhFlowViewportAPILocator() {}
};

namespace
{
    //Callback when an attribute of this Maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* dataProducerSceneIndexData)
    {
        if (! dataProducerSceneIndexData){
            return;
        }

        MhFlowViewportAPILocator* flowViewportAPIMayaLocator = reinterpret_cast<MhFlowViewportAPILocator*>(dataProducerSceneIndexData);

        MPlug parentPlug = plug.parent();
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

        MhFlowViewportAPILocator* flowViewportAPIMayaLocator = reinterpret_cast<MhFlowViewportAPILocator*>(clientData);
        flowViewportAPIMayaLocator->setCubeGridParametersFromAttributes();
        flowViewportAPIMayaLocator->addedToModelCb();
    }

    void nodeAddedToModel(MObject& node, void* /* clientData */)
    {
        auto fpNode = reinterpret_cast<MhFlowViewportAPILocator*>(MFnDagNode(node).userNode());
        if (!TF_VERIFY(fpNode)) {
            return;
        }

        fpNode->addedToModelCb();
    }

    void nodeRemovedFromModel(MObject& node, void* /* clientData */)
    {
        auto fpNode = reinterpret_cast<MhFlowViewportAPILocator*>(MFnDagNode(node).userNode());
        if (!TF_VERIFY(fpNode)) {
            return;
        }

        fpNode->removedFromModelCb();
    }

}//end of anonymous namespace

//Initialization of static members
MTypeId MhFlowViewportAPILocator::id( 0x58000086 );
MString	MhFlowViewportAPILocator::nodeClassification("drawdb/subscene/MhFlowViewportAPILocator");
MObject MhFlowViewportAPILocator::mNumCubeLevelsX;
MObject MhFlowViewportAPILocator::mNumCubeLevelsY;
MObject MhFlowViewportAPILocator::mNumCubeLevelsZ;
MObject MhFlowViewportAPILocator::mCubeHalfSize;
MObject MhFlowViewportAPILocator::mCubeInitalTransform;
MObject MhFlowViewportAPILocator::mCubeColor;
MObject MhFlowViewportAPILocator::mCubeOpacity;
MObject MhFlowViewportAPILocator::mCubesUseInstancing;
MObject MhFlowViewportAPILocator::mCubesDeltaTrans;

void MhFlowViewportAPILocator::postConstructor() 
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

    //Create a Filtering scene index client
    _hydraViewportFilteringSceneIndexClientExample = std::make_shared<Fvp::FilteringSceneIndexClientExample>(
                                            "FilteringSceneIndexClientExample", 
                                            Fvp::FilteringSceneIndexClient::Category::kSceneFiltering, 
                                            FvpViewportAPITokens->allRenderers, //We could set only Storm by using "GL" or only Arnold by using "Arnold" or both with "GL, Arnold"
                                            nullptr);//DCC node will be filled later
    
    setCubeGridParametersFromAttributes();

    MObject obj = thisMObject();
    _nodeAddedToModelCbId = MModelMessage::addNodeAddedToModelCallback(obj, nodeAddedToModel);
    _nodeRemovedFromModelCbId = MModelMessage::addNodeRemovedFromModelCallback(obj, nodeRemovedFromModel);
}

//This is called only when our node is destroyed and the undo queue flushed.
MhFlowViewportAPILocator::~MhFlowViewportAPILocator() 
{ 
    //Remove the callbacks
    for(auto cbId : {_cbAfterOpenId, _cbAttributeChangedId, _nodeAddedToModelCbId, _nodeRemovedFromModelCbId}) {
        if (cbId) {
            CHECK_MSTATUS(MMessage::removeCallback(cbId));
        }
    }

    //The DataProducerSceneIndexExample in its destructor removes itself by calling DataProducerSceneIndexExample::RemoveDataProducerSceneIndex()
     
    //Unregister filtering scene index client
    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    filteringSceneIndexInterface.unregisterFilteringSceneIndexClient(_hydraViewportFilteringSceneIndexClientExample);

    //Unregister viewport information client
    Fvp::InformationInterface& informationInterface = Fvp::InformationInterface::Get();
    informationInterface.UnregisterInformationClient(_hydraViewportInformationClient);
}

void MhFlowViewportAPILocator::setCubeGridParametersFromAttributes()
{
    MObject mObj = thisMObject();
    if (mObj.isNull()){
        return;
    }

    GetAttributeValue(_cubeGridParams._numLevelsX, mObj, MhFlowViewportAPILocator::mNumCubeLevelsX);
    GetAttributeValue(_cubeGridParams._numLevelsY, mObj, MhFlowViewportAPILocator::mNumCubeLevelsY);
    GetAttributeValue(_cubeGridParams._numLevelsZ, mObj, MhFlowViewportAPILocator::mNumCubeLevelsZ);

    GetAttributeValue(_cubeGridParams._halfSize, mObj, MhFlowViewportAPILocator::mCubeHalfSize);
    
    MMatrix mat;
    GetMatrixAttributeValue(mat, mObj, MhFlowViewportAPILocator::mCubeInitalTransform);
    memcpy(_cubeGridParams._initalTransform.GetArray(), mat[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d

    double3 color;
    GetDouble3AttributeValue(color, mObj, MhFlowViewportAPILocator::mCubeColor);
    _cubeGridParams._color.data()[0]        = color[0];//Implicit conversion from double to float
    _cubeGridParams._color.data()[1]        = color[1];
    _cubeGridParams._color.data()[2]        = color[2];

    GetAttributeValue(_cubeGridParams._opacity, mObj, MhFlowViewportAPILocator::mCubeOpacity);
    GetAttributeValue(_cubeGridParams._useInstancing, mObj, MhFlowViewportAPILocator::mCubesUseInstancing);

    double3 deltaTrans;
    GetDouble3AttributeValue(deltaTrans, mObj, MhFlowViewportAPILocator::mCubesDeltaTrans);
    _cubeGridParams._deltaTrans.data()[0]   = deltaTrans[0];//Implicit conversion from double to float
    _cubeGridParams._deltaTrans.data()[1]   = deltaTrans[1];
    _cubeGridParams._deltaTrans.data()[2]   = deltaTrans[2];

    _hydraViewportDataProducerSceneIndexExample.setCubeGridParams(_cubeGridParams);
}

MStatus MhFlowViewportAPILocator::compute( const MPlug& plug, MDataBlock& dataBlock)
{
    return MS::kSuccess;
}

bool MhFlowViewportAPILocator::isBounded() const
{
    return true;
}

//We return as a bounding box the bounding box of the data producer hydra data
MBoundingBox MhFlowViewportAPILocator::boundingBox() const
{
    float corner1X, corner1Y, corner1Z, corner2X, corner2Y, corner2Z;
    _hydraViewportDataProducerSceneIndexExample.getPrimsBoundingBox(corner1X, corner1Y, corner1Z, corner2X, corner2Y, corner2Z);
    return MBoundingBox( {corner1X, corner1Y, corner1Z}, {corner2X, corner2Y, corner2Z});
}

// Called before this node is evaluated by Evaluation Manager
MStatus MhFlowViewportAPILocator::preEvaluation(
    const MDGContext& context,
    const MEvaluationNode& evaluationNode)
{
    return MStatus::kSuccess;
}

void MhFlowViewportAPILocator::getCacheSetup(const MEvaluationNode& evalNode, MNodeCacheDisablingInfo& disablingInfo, MNodeCacheSetupInfo& cacheSetupInfo, MObjectArray& monitoredAttributes) const
{
    MPxSurfaceShape::getCacheSetup(evalNode, disablingInfo, cacheSetupInfo, monitoredAttributes);
    assert(!disablingInfo.getCacheDisabled());
    cacheSetupInfo.setPreference(MNodeCacheSetupInfo::kWantToCacheByDefault, true);
}

void* MhFlowViewportAPILocator::creator()
{
    static const MString errorMsg ("You need to load the mayaHydra plugin before creating this node");

    int	isMayaHydraLoaded = false;
    // Validate that the mayaHydra plugin is loaded.
    MGlobal::executeCommand( "pluginInfo -query -loaded mayaHydra", isMayaHydraLoaded );
    if( ! isMayaHydraLoaded){
        MGlobal::displayError(errorMsg);
        return nullptr;
    }

    return new MhFlowViewportAPILocator;
}

void MhFlowViewportAPILocator::addedToModelCb()
{
    static const SdfPath noPrefix = SdfPath::AbsoluteRootPath();

    //Add the callback when an attribute of this node changes
    MObject obj = thisMObject();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(obj, attributeChangedCallback, ((void*)this));

    _hydraViewportDataProducerSceneIndexExample.setContainerNode(&obj);
    _hydraViewportDataProducerSceneIndexExample.addDataProducerSceneIndex();

    //Store the MObject* of the maya node in various classes
    _hydraViewportFilteringSceneIndexClientExample->setDccNode(&obj);

    //Register this filtering scene index client, so it can append custom filtering scene indices to Hydra viewport scene indices
    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    const bool bResult = filteringSceneIndexInterface.registerFilteringSceneIndexClient(_hydraViewportFilteringSceneIndexClientExample);
    if(! bResult){
        perror("ERROR : filteringSceneIndexInterface.registerFilteringSceneIndexClient returned false");
    }
}

void MhFlowViewportAPILocator::removedFromModelCb()
{
    //Remove the callback
    if (_cbAttributeChangedId){
        CHECK_MSTATUS(MMessage::removeCallback(_cbAttributeChangedId));
        _cbAttributeChangedId = 0;
    }

    //Remove the data producer scene index.
    _hydraViewportDataProducerSceneIndexExample.removeDataProducerSceneIndex();
    
    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    filteringSceneIndexInterface.unregisterFilteringSceneIndexClient(_hydraViewportFilteringSceneIndexClientExample);
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Plugin Registration
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
//Macro to create input attribute for the maya node
#define MAKE_INPUT(attr)	\
    CHECK_MSTATUS(attr.setKeyable(true) );		\
    CHECK_MSTATUS(attr.setStorable(true) );		\
    CHECK_MSTATUS(attr.setReadable(true) );		\
    CHECK_MSTATUS(attr.setWritable(true) );		\
	CHECK_MSTATUS(attr.setAffectsAppearance(true) );

//Macro to create output attribute for the maya node
#define MAKE_OUTPUT(attr)	\
    CHECK_MSTATUS ( attr.setKeyable(false) ); \
    CHECK_MSTATUS ( attr.setStorable(false) );	\
    CHECK_MSTATUS ( attr.setReadable(true) ); \
    CHECK_MSTATUS ( attr.setWritable(false) );

MStatus MhFlowViewportAPILocator::initialize()
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

    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsX));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsY));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsZ));
    CHECK_MSTATUS ( addAttribute(mCubeHalfSize));
    CHECK_MSTATUS ( addAttribute(mCubeInitalTransform));
    CHECK_MSTATUS ( addAttribute(mCubeColor));
    CHECK_MSTATUS ( addAttribute(mCubeOpacity));
    CHECK_MSTATUS ( addAttribute(mCubesUseInstancing));
    CHECK_MSTATUS ( addAttribute(mCubesDeltaTrans));
    
    return status;
}

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    static const char * pluginVersion = "1.0";
    MFnPlugin plugin( obj, PLUGIN_COMPANY, pluginVersion, "Any");

    status = plugin.registerShape(
                "MhFlowViewportAPILocator",
                MhFlowViewportAPILocator::id,
                &MhFlowViewportAPILocator::creator,
                &MhFlowViewportAPILocator::initialize,
                &MhFlowViewportAPILocator::nodeClassification);
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

    status = plugin.deregisterNode( MhFlowViewportAPILocator::id );
    if (!status) {
        status.perror("deregisterNode");
        return status;
    }
    return status;
}
