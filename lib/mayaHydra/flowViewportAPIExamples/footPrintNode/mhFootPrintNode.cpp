//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

////////////////////////////////////////////////////////////////////////
// DESCRIPTION: 
// 
// This plug-in demonstrates how to draw a simple mesh like foot Print in an easy way within a Hydra viewport.
// This node is only visible in a Hydra viewport, it won't be visible in viewport 2.0.
//
// For comparison, you can reference a Maya Developer Kit sample named footPrintNode which uses Viewport 2.0 override to draw.
// To create an instance of this node in maya, please use the following MEL command :
// 
//  createNode("MhFootPrint")
//
////////////////////////////////////////////////////////////////////////

//Local headers
#include "mhFootPrintNodeStrings.h"

//maya headers
#include <maya/MPxLocatorNode.h>
#include <maya/MString.h>
#include <maya/MTypeId.h>
#include <maya/MPlug.h>
#include <maya/MFnPlugin.h>
#include <maya/MDistance.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MSceneMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MGlobal.h>

//Flow viewport headers
#include <flowViewport/API/fvpVersionInterface.h>
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>

//Hydra headers
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Node implementation with Hydra scene index
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class MhFootPrint : public MPxLocatorNode
{
public:
    MhFootPrint();
    ~MhFootPrint() override;

    //Is called when the MObject has been constructed and is valid
    void    postConstructor() override;

    MStatus   		compute( const MPlug& plug, MDataBlock& data ) override;

    bool            isBounded() const override;
    MBoundingBox    boundingBox() const override;

    void setupFlowViewportInterface();
    void UpdateFootPrintPrims();
    void TriggerACallToCompute();
    
    static  void *      creator();
    static  MStatus     initialize();

    //Attributes
    static MObject     mSize;
    static MObject     mWorldS;
    static MObject     mColor;
    static MObject     mDummyInput; //Dummy input to trigger a call to compute
    static MObject     mDummyOutput;//Dummy output to trigger a call to compute

    static	MTypeId		id;
    static	MString		nodeClassification;

private:
    ///get the value of the size attribute in centimeters
    float _GetSizeInCentimeters() const;
    ///get the value of the color attribute, returned as a 3D Hydra vector 
    GfVec3f _GetColor() const;
    ///Create the Hydra foot print primitives
    void _CreateAndAddFootPrintPrimitives();
    ///Remove the Hydra foot print primitives
    void _RemoveFootPrintPrimitives();
    ///Update the MObject of this node
    void _UpdateThisMObject();

    ///Counter to make the hydra primitives unique
    static std::atomic_int _counter;

    /// Sole path to be used in the retained hydra scene index for the sole primitive
    SdfPath                     _solePath;
    /// Heel path to be used in the retained hydra scene index for the heel primitive
    SdfPath                     _heelPath;

    ///Hydra retained scene index to add the 2 foot print primitives
    HdRetainedSceneIndexRefPtr  _retainedSceneIndex  {nullptr};

    ///To be used in hydra viewport API to pass the Maya node's MObject for setting callbacks for data producer scene indices
    MObjectHandle                _thisMObject; 
    ///To check if the MObject of this node has changed
    MObject                     _oldMObject; 
    ///To hold the afterOpenCallback Id to be able to react when a File Open has happened.
    MCallbackId                 _cbAfterOpenId = 0;
    ///To hold the attributeChangedCallback Id to be able to react when the 3D grid creation parameters attributes from this node change.
    MCallbackId                 _cbAttributeChangedId = 0;
};

namespace 
{
    // Foot print data
    static const VtArray<GfVec3f> solePoints = { 
                               {  0.00f, 0.0f, -0.70f },
                               {  0.04f, 0.0f, -0.69f },
                               {  0.09f, 0.0f, -0.65f },
                               {  0.13f, 0.0f, -0.61f },
                               {  0.16f, 0.0f, -0.54f },
                               {  0.17f, 0.0f, -0.46f },
                               {  0.17f, 0.0f, -0.35f },
                               {  0.16f, 0.0f, -0.25f },
                               {  0.15f, 0.0f, -0.14f },
                               {  0.13f, 0.0f,  0.00f },
                               {  0.00f, 0.0f,  0.00f },
                               { -0.13f, 0.0f,  0.00f },
                               { -0.15f, 0.0f, -0.14f },
                               { -0.16f, 0.0f, -0.25f },
                               { -0.17f, 0.0f, -0.35f },
                               { -0.17f, 0.0f, -0.46f },
                               { -0.16f, 0.0f, -0.54f },
                               { -0.13f, 0.0f, -0.61f },
                               { -0.09f, 0.0f, -0.65f },
                               { -0.04f, 0.0f, -0.69f },
                               { -0.00f, 0.0f, -0.70f } 
                             };

    static const VtArray<GfVec3f> heelPoints= { 
                               {  0.00f, 0.0f,  0.06f },
                               {  0.13f, 0.0f,  0.06f },
                               {  0.14f, 0.0f,  0.15f },
                               {  0.14f, 0.0f,  0.21f },
                               {  0.13f, 0.0f,  0.25f },
                               {  0.11f, 0.0f,  0.28f },
                               {  0.09f, 0.0f,  0.29f },
                               {  0.04f, 0.0f,  0.30f },
                               {  0.00f, 0.0f,  0.30f },
                               { -0.04f, 0.0f,  0.30f },
                               { -0.09f, 0.0f,  0.29f },
                               { -0.11f, 0.0f,  0.28f },
                               { -0.13f, 0.0f,  0.25f },
                               { -0.14f, 0.0f,  0.21f },
                               { -0.14f, 0.0f,  0.15f },
                               { -0.13f, 0.0f,  0.06f },
                               { -0.00f, 0.0f,  0.06f } 
                            };
    
    //Number of sole triangles is soleVertsCount - 2
    static const VtIntArray soleFaceVertexCounts    =   {
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3
                                                    };

    //Number of heel triangles is heelVertsCount - 2
    static const VtIntArray heelFaceVertexCounts    =   {
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3
                                                    };

    static const VtIntArray soleFaceVertexIndices   = {
                                                        2,  1,  0,
                                                        3,  2,  0,
                                                        4,  3,  0,
                                                        5,  4,  0,
                                                        6,  5,  0,
                                                        7,  6,  0,
                                                        8,  7,  0,
                                                        9,  8,  0,
                                                        10, 9,  0,
                                                        11, 10, 0,
                                                        12, 11, 0,
                                                        13, 12, 0,
                                                        14, 13, 0,
                                                        15, 14, 0,
                                                        16, 15, 0,
                                                        17, 16, 0,
                                                        18, 17, 0,
                                                        19, 18, 0,
                                                        20, 19, 0
                                                       };

    static const VtIntArray heelFaceVertexIndices   = {
                                                        2,  1,  0,
                                                        3,  2,  0,
                                                        4,  3,  0,
                                                        5,  4,  0,
                                                        6,  5,  0,
                                                        7,  6,  0,
                                                        8,  7,  0,
                                                        9,  8,  0,
                                                        10, 9,  0,
                                                        11, 10, 0, 
                                                        12, 11, 0, 
                                                        13, 12, 0, 
                                                        14, 13, 0, 
                                                        15, 14, 0,
                                                        16, 15, 0
                                                      };

    //For the maya bounding box
    static const MPoint corner1( -0.17, 0.0, -0.7 );
    static const MPoint corner2( 0.17, 0.0, 0.3 );
    
    //Create the Hydra primitive and add it to the retained scene index
    void _CreateAndAddPrim(const HdRetainedSceneIndexRefPtr& retainedSceneIndex, const SdfPath& primPath, const VtArray<GfVec3f>& points, const VtIntArray& faceVertexCount, const VtIntArray& faceVertexIndices, const GfVec3f& scale, const GfVec3f& displayColor)
    {
        using _PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;
        using _IntArrayDs   = HdRetainedTypedSampledDataSource<VtIntArray>;

        const _IntArrayDs::Handle fvcDs = _IntArrayDs::New(faceVertexCount);
        const _IntArrayDs::Handle fviDs = _IntArrayDs::New(faceVertexIndices);
        
        const VtIntArray vertexColorArray(points.size(), 0);//Is an index in the vertex color array, 1 per vertex,but  we only have one same color for all verts (index 0)

        const HdContainerDataSourceHandle meshDs =
                HdMeshSchema::Builder()
                    .SetTopology(HdMeshTopologySchema::Builder()
                        .SetFaceVertexCounts(fvcDs)
                        .SetFaceVertexIndices(fviDs)
                        .Build())
                   .SetDoubleSided(HdRetainedTypedSampledDataSource<bool>::New(true))//Make the mesh double sided
                   .Build();

        const HdContainerDataSourceHandle primvarsDs =
            HdRetainedContainerDataSource::New(

                //Create the vertices positions
                HdPrimvarsSchemaTokens->points,
                HdPrimvarSchema::Builder()
                    .SetPrimvarValue(_PointArrayDs::New(points))
                    .SetInterpolation(HdPrimvarSchema::
                        BuildInterpolationDataSource(
                            HdPrimvarSchemaTokens->vertex))
                    .SetRole(HdPrimvarSchema::
                        BuildRoleDataSource(
                            HdPrimvarSchemaTokens->point))
                    .Build(),

                //Create the vertex colors
                HdTokens->displayColor,
                HdPrimvarSchema::Builder()
                    .SetIndexedPrimvarValue(
                        HdRetainedTypedSampledDataSource<VtVec3fArray>::New(
                            VtVec3fArray{
                                displayColor, 
                            }))
                    .SetIndices(
                        HdRetainedTypedSampledDataSource<VtIntArray>::New(
                                 vertexColorArray
                        )
                    )
                    .SetInterpolation(
                        HdPrimvarSchema::BuildInterpolationDataSource(
                            HdPrimvarSchemaTokens->varying))
                    .SetRole(
                        HdPrimvarSchema::BuildRoleDataSource(
                            HdPrimvarSchemaTokens->color))//vertex color
                    .Build()
            );

        //Apply the size of the prim as a scale matrix
        GfMatrix4d transform;
        transform.SetIdentity();
        transform.SetScale(scale);

        //Create the primitive
        HdRetainedSceneIndex::AddedPrimEntry addedPrim;
        addedPrim.primPath   = primPath;
        addedPrim.primType   = HdPrimTypeTokens->mesh;
        addedPrim.dataSource = HdRetainedContainerDataSource::New(
            //Create a matrix
            HdXformSchemaTokens->xform,
            HdXformSchema::Builder()
                    .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(
                    transform)).Build(),

            //create a mesh
            HdMeshSchemaTokens->mesh,
            meshDs,
            HdPrimvarsSchemaTokens->primvars,
            primvarsDs
        );

        //Add the prim in the retained scene index
        retainedSceneIndex->AddPrims({addedPrim});
    }

    //Callback when an attribute of the maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* footPrintData)
    {
        if (! footPrintData){
            return;
        }

        MhFootPrint* footPrint = reinterpret_cast<MhFootPrint*>(footPrintData);

        MPlug parentPlug = plug.parent();
        if (
            (plug       == MhFootPrint::mSize)  ||
            (parentPlug == MhFootPrint::mColor) ||
            (plug       == MhFootPrint::mColor)
           ){
                footPrint->UpdateFootPrintPrims();
        }
    }

    //Get the color attribute value which is a double3
    void GetDouble3AttributeValue(double3& outVal, const MObject& node, const MObject& attr)
    {
        MPlug plug(node, attr);
        if (plug.isNull()){
            return;
        }

        MObject oDouble3;
        plug.getValue(oDouble3);

        MFnNumericData fnData(oDouble3);
        fnData.getData( outVal[0], outVal[1], outVal[2] );
    }

    // Register all strings used by the plugin C++ code
    static MStatus registerMStringResources()
    {
	    MStringResource::registerString(rMayaHydraNotLoadedStringError);
	    return MS::kSuccess;
    }

}//end of anonymous namespace

//Static variables init
std::atomic_int MhFootPrint::_counter {0};
MObject MhFootPrint::mSize;
MObject MhFootPrint::mColor;
MTypeId MhFootPrint::id( 0x58000087 );
MString	MhFootPrint::nodeClassification("hydraAPIExample/geometry/footPrint");
MObject MhFootPrint::mWorldS;
MObject MhFootPrint::mDummyInput;
MObject MhFootPrint::mDummyOutput;

namespace {
    //Callback after a File Open
    void afterOpenCallback (void *clientData) 
    {
        if (! clientData){
            return;
        }

        //Trigger a call to compute so that everything is initialized
        MhFootPrint* footPrintInstance = reinterpret_cast<MhFootPrint*>(clientData);
        footPrintInstance->TriggerACallToCompute();
    }
}

MhFootPrint::MhFootPrint()
{
}

void MhFootPrint::postConstructor()
{
    //We have a valid MObject in this function
    _solePath = SdfPath(std::string("/sole_") + std::to_string(_counter));
    _heelPath = SdfPath(std::string("/heel_") + std::to_string(_counter));
    _counter++;

    //Add a callback after a load scene
    _cbAfterOpenId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, afterOpenCallback, ((void*)this)) ;

    _retainedSceneIndex = HdRetainedSceneIndex::New();

    _CreateAndAddFootPrintPrimitives();
}

MhFootPrint::~MhFootPrint()
{
    //Remove the callbacks
    if (_cbAfterOpenId){
        CHECK_MSTATUS(MSceneMessage::removeCallback(_cbAfterOpenId));
        _cbAfterOpenId = 0;
    }
    
    if (_cbAttributeChangedId){
        CHECK_MSTATUS(MMessage::removeCallback(_cbAttributeChangedId));
        _cbAttributeChangedId = 0;
    }

    //Remove our retained scene index from hydra
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeViewportDataProducerSceneIndex(_retainedSceneIndex, pxr::FvpViewportAPITokens->allViewports);
}

//Create the Hydra foot print primitives in the retained scene index
void MhFootPrint::_CreateAndAddFootPrintPrimitives() 
{ 
    //Get the value of the size and color attributes
    const float fSize           = _GetSizeInCentimeters();
    const GfVec3f displayColor  = _GetColor();
    const GfVec3f scale         = {fSize,fSize,fSize};//convert size into a 3d uniform scale which we'll convert into a scale matrix

    _CreateAndAddPrim(_retainedSceneIndex, _solePath, solePoints, soleFaceVertexCounts, soleFaceVertexIndices, scale, displayColor);
    _CreateAndAddPrim(_retainedSceneIndex, _heelPath, heelPoints, heelFaceVertexCounts, heelFaceVertexIndices, scale, displayColor);
}

//Remove the 2 primitives from the retained scene index
void MhFootPrint::_RemoveFootPrintPrimitives() 
{ 
    _retainedSceneIndex->RemovePrims({_solePath, _heelPath});
}

//To update we need to remove the previous primitives and create new ones
void MhFootPrint::UpdateFootPrintPrims() 
{ 
    _RemoveFootPrintPrimitives();
    _CreateAndAddFootPrintPrimitives();
}

void MhFootPrint::TriggerACallToCompute() 
{ 
    {
        MPlug plug(thisMObject(), mDummyInput);
        if (!plug.isNull())
        {
            int dummyInputVal;
            if (plug.getValue(dummyInputVal))
            {
                plug.setValue(dummyInputVal + 1);//Dirty one parameter that affects the mDummyOutput attribute
                MPlug plugOutput(thisMObject(), mDummyOutput);
                if (!plugOutput.isNull())
                {
                    int dummyOutputVal;
                    plugOutput.getValue(dummyOutputVal);//This will trigger a call to compute
                }
            }
        }
    }
}

void MhFootPrint::_UpdateThisMObject()
{
    if (_thisMObject.isValid()){
        return;
    }

    _thisMObject = thisMObject();
}

void MhFootPrint::setupFlowViewportInterface()
{
    static const SdfPath noPrefix = SdfPath::AbsoluteRootPath();

    _UpdateThisMObject();

    //Remove the callback
    if (_cbAttributeChangedId){
        CHECK_MSTATUS(MMessage::removeCallback(_cbAttributeChangedId));
        _cbAttributeChangedId = 0;
    }
    //Add the callback when an attribute of this node changes
    MObject obj = _thisMObject.object();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(obj, attributeChangedCallback, ((void*)this));

    //Remove the previous data producer scene index in case it was registered, if that occurs it's because _thisMObject has changed and we want to update the maya callbacks on the node
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeViewportDataProducerSceneIndex(_retainedSceneIndex, pxr::FvpViewportAPITokens->allViewports);

    //Data producer scene index interface is used to add the retained scene index to all viewports with all render delegates
    dataProducerSceneIndexInterface.addDataProducerSceneIndex(_retainedSceneIndex, noPrefix, (void*)&obj, FvpViewportAPITokens->allViewports,FvpViewportAPITokens->allRenderers);
}

// Retrieve value of the size attribute from the node
float MhFootPrint::_GetSizeInCentimeters() const
{
    const MObject obj = thisMObject();
    
    MPlug plug(obj, MhFootPrint::mSize);
    if (!plug.isNull())
    {
        MDistance sizeVal;
        if (plug.getValue(sizeVal))
        {
            return (float)sizeVal.asCentimeters();
        }
    }

    return 1.0f;
}

// Retrieve value of the color attribute from the node
GfVec3f MhFootPrint::_GetColor() const
{
    const MObject obj = thisMObject();
    MPlug plug(obj, MhFootPrint::mColor);
    if (!plug.isNull())
    {
        double3 color;
        GetDouble3AttributeValue(color, obj, MhFootPrint::mColor);
        return {(float)color[0], (float)color[1], (float)color[2]};
    }

    return GfVec3f(0.f,0.f,1.f);
}

MStatus MhFootPrint::compute( const MPlug& plug, MDataBlock& dataBlock)
{
    //The MObject can change if the node gets deleted and deletion being undone
    if (! _thisMObject.isValid()){
        setupFlowViewportInterface();
    }

    if (plug == mWorldS) 
    {
        if (plug.isElement())
        {
            MArrayDataHandle outputArrayHandle = dataBlock.outputArrayValue( mWorldS );
            outputArrayHandle.setAllClean();
        }
        dataBlock.setClean(plug);

        return MS::kSuccess;
    }
    
    return MS::kUnknownParameter;;
}

bool MhFootPrint::isBounded() const
{
    return true;
}

MBoundingBox MhFootPrint::boundingBox() const
{
    const double multiplier = _GetSizeInCentimeters();
    return MBoundingBox( corner1 * multiplier, corner2 * multiplier);//corner1 and 2 are the bounding box corner of our geometry
}

void* MhFootPrint::creator()
{
    int	isMayaHydraLoaded = false;
    // Validate that the mayaHydra plugin is loaded.
    MGlobal::executeCommand( "pluginInfo -query -loaded mayaHydra", isMayaHydraLoaded );
    if( ! isMayaHydraLoaded){
        MStatus status;
	    MString errorString = MStringResource::getString(rMayaHydraNotLoadedStringError, status);	     
        if (! status){
            status.perror("Cannot retrieve the rMayaHydraNotLoadedStringError string, but you need to load mayaHydra before creating this node");
        }else{
	        MGlobal::displayError(errorString);	    
        }
        return nullptr;
    }

    return new MhFootPrint();
}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Plugin Registration
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------

//Macro to create input attribute for the maya node
#define MAKE_INPUT(attr)	                    \
    CHECK_MSTATUS(attr.setKeyable(true) );		\
    CHECK_MSTATUS(attr.setStorable(true) );		\
    CHECK_MSTATUS(attr.setReadable(true) );		\
    CHECK_MSTATUS(attr.setWritable(true) );		\
	CHECK_MSTATUS(attr.setAffectsAppearance(true) );

//Macro to create output attribute for the maya node
#define MAKE_OUTPUT(attr)			            \
    CHECK_MSTATUS ( attr.setKeyable(false) );   \
    CHECK_MSTATUS ( attr.setStorable(false) );	\
    CHECK_MSTATUS ( attr.setReadable(true) );   \
    CHECK_MSTATUS ( attr.setWritable(false) );

MStatus MhFootPrint::initialize()
{
    MFnUnitAttribute    unitFn;
    MFnNumericAttribute nAttr;

    mSize = unitFn.create( "size", "sz", MFnUnitAttribute::kDistance);
    MAKE_INPUT(unitFn);
    CHECK_MSTATUS ( unitFn.setDefault(1.0) );

    mWorldS = unitFn.create("worldS", "ws", MFnUnitAttribute::kDistance, 1.0);
    unitFn.setWritable(true);
    unitFn.setCached(false);
    unitFn.setArray( true );
    unitFn.setUsesArrayDataBuilder( true );
    unitFn.setWorldSpace( true );

    mColor = nAttr.create("color", "col", MFnNumericData::k3Double, 1.0);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(0.0, 0.0, 1.0) );

    //Create dummy input attribute to trigger a call to the compute function on demand. as it's in the compute fonction that we add our scene indices
    mDummyInput = nAttr.create("dummyInput", "dI", MFnNumericData::kInt, 1.0);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(1) );

    //Create dummy output attribute to trigger a call to the compute function on demand. as it's in the compute fonction that we add our scene indices
    mDummyOutput = nAttr.create("dummyOutput", "dO", MFnNumericData::kInt, 1.0);
    MAKE_OUTPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(1) );

    CHECK_MSTATUS ( addAttribute(mSize) );
    CHECK_MSTATUS ( addAttribute(mColor));
    CHECK_MSTATUS ( addAttribute(mDummyInput));
    CHECK_MSTATUS ( addAttribute(mDummyOutput));
    CHECK_MSTATUS ( addAttribute(mWorldS));
    
    CHECK_MSTATUS ( attributeAffects(mSize, mWorldS));
    CHECK_MSTATUS ( attributeAffects(mDummyInput, mDummyOutput));
    return MS::kSuccess;
}

MStatus initializePlugin( MObject obj )
{
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "2025.0", "Any");
    
    MStatus   status;
    // This is done first, so the strings are available. 
	status = plugin.registerUIStrings(registerMStringResources, "mayaHydraFootPrintNodeInitStrings");
	if (status != MS::kSuccess)
	{
		status.perror("registerUIStrings");
		return status;
	}
    
    status = plugin.registerNode(
                kMhFootPrintNodePluginId,
                MhFootPrint::id,
                &MhFootPrint::creator,
                &MhFootPrint::initialize,
                MPxNode::kLocatorNode,
                &MhFootPrint::nodeClassification);
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

    status = plugin.deregisterNode( MhFootPrint::id );
    if (!status) {
        status.perror("deregisterNode");
        return status;
    }
    return status;
}
