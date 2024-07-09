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
// This plug-in demonstrates how to draw within a hydra viewport a simple quad mesh using a custom GLSLFX shader for HdStorm.
// The custom shader is defined in in the flowViewportShadersDiscoveryPlugin project.
// This maya node is only visible in a Hydra viewport as it creates hydra primitives, no maya geometry, so it's not visible in viewport 2.0.
//
// To create an instance of this node in maya, please use the following MEL command :
// 
//  createNode("MhCustomShaders")
//
////////////////////////////////////////////////////////////////////////

//maya headers
#include <maya/MPxLocatorNode.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnUnitAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MSceneMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MModelMessage.h>

//Flow viewport headers
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>

//Hydra headers
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

// Maya Hydra headers
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndexUtils.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

TF_DEFINE_PRIVATE_TOKENS(_tokens, 
    (FVP_CustomBasicLightingShader) //Name of the shader loaded by Hydra
    (FVP_CustomColor) //Parameter of our custom color GLSLFX shader
    (st) //Primary uv set usual name in USD
    (uvPrimVarReaderNode)//Name of the prim var reader node
    (displayColorPrimVarReaderNode)//Name of the prim var reader node
    (varname)
    (result)
    (fallback)
);

static const TfToken purposes[] = { HdMaterialBindingsSchemaTokens->allPurpose };

//Quad vertices
static const VtVec3fArray gPrimPoints = 
        { 
            { -1.0f, 0.0f, -1.0f },   { 1.0f, 0.0f, -1.0f },  
            {  1.0f, 0.0f,  1.0f },   { -1.0f, 0.0f, 1.0f }
        };

//Quad vertices count per face (only 1 quad face)
static const VtIntArray gPrimFaceVertexCounts = {4};

//Quad face vertex index for our quad face
static const VtIntArray gPrimFaceVertexIndices =   
        { 
            0, 1, 2, 3,
        };

//Quad UVs per vertex
static const VtVec2fArray gPrimUVs =     {   { 0.0f, 0.0f},
                                             { 1.0f, 0.0f},
                                             { 1.0f, 1.0f},
                                             { 0.0f, 1.0f} 
                                        };

//Quad color per vertex 
static const VtVec3fArray gPrimVertexColors =    {
                                                        {1.f, 1.f, 1.f},
                                                        {1.f, 0.f, 0.f},
                                                        {1.f, 1.f, 1.f},
                                                        {0.f, 0.f, 1.f}
                                                    };

//Quad bounding box for the maya object
static const MPoint corner1(-1.0, 0.0, -1.0);
static const MPoint corner2(1.0, 0.0, 1.0);

void nodeAddedToModel(MObject& node, void* clientData);
void nodeRemovedFromModel(MObject& node, void* clientData);

//Helper function to create a material node and its material relationship for a material network
void _AddPrimVarsMaterialNode(
    HdMaterialNetwork& network,
    const SdfPath& matPath,
    const TfToken   primVarName,
    const TfToken   primVarId,
    const SdfPath&  primvarNodePath,
    const VtValue&  fallbackValue)
{
    // Create the primvar reader node
    HdMaterialNode primvarNode;
    primvarNode.path                            = primvarNodePath;
    primvarNode.identifier                      = primVarId;
    primvarNode.parameters[_tokens->varname]    = primVarName;
    primvarNode.parameters[_tokens->fallback]   = fallbackValue;
    network.primvars.push_back(primVarName);
    network.nodes.emplace_back(std::move(primvarNode));

    // Insert connection between primvar reader node
    // and the material input
    HdMaterialRelationship primvarRel;
    primvarRel.inputId      = primvarNodePath;
    primvarRel.inputName    = _tokens->result;
    primvarRel.outputId     = matPath;
    primvarRel.outputName   = primVarName;
    network.relationships.emplace_back(std::move(primvarRel));
}

//Create the material network which will use our custom shader
VtValue _CreateHydraCustomBasicLightingMaterial(HdMaterialNetworkMap& networkMap, const SdfPath& matPath, const GfVec3f& color)
{
    TfToken const&       terminalType = HdMaterialTerminalTokens->surface;
    HdMaterialNetwork&   network = networkMap.map[terminalType];

    //Create our custom GLSLFX shader node
    HdMaterialNode       terminal;
    //Identifier of our custom GLSLFX shader. "FVP_CustomBasicLightingShader" is the name of the shader defined in the flowViewportShadersDiscoveryProject shadersDef.usda file, names have to match exactly
    //Using this will make hydra look into its database of shaders and find it in the glslfx database.
    terminal.identifier = _tokens->FVP_CustomBasicLightingShader;
    terminal.path       = matPath;

    // Add the shaders parameters. "FVP_CustomColor" is defined as a parameter in the flowViewportShadersDiscoveryProject shadersDef.usda file, names have to match exactly
    terminal.parameters[_tokens->FVP_CustomColor] = VtValue(GfVec3f(color[0], color[1], color[2]));
    
    //Add the primvars readers to be able to access primvars in the shader 
    _AddPrimVarsMaterialNode(
        network,
        matPath,
        HdTokens->displayColor,//This will enable the HdGet_displayColor() function to get a vec3 for vertex colors
        UsdImagingTokens->UsdPrimvarReader_float3,
        matPath.AppendChild(_tokens->displayColorPrimVarReaderNode),
        VtValue(GfVec3f(1.0f, 1.0f, 1.0f))
    );
    _AddPrimVarsMaterialNode(
        network,
        matPath,
        _tokens->st,//This will enable the HdGet_st() function to get a vec2 for vertex colors, only if you use a texture node.
        UsdImagingTokens->UsdPrimvarReader_float2,
        matPath.AppendChild(_tokens->uvPrimVarReaderNode),
        VtValue(GfVec2f(1.0f, 1.0f))
    );
    
    // Insert terminal and update material network
    networkMap.terminals.push_back(terminal.path);
    network.nodes.emplace_back(std::move(terminal));

    return VtValue(networkMap);
}

// Create the Hydra primitive and add it to the retained scene index
void _CreateAndAddPrim(
    const HdRetainedSceneIndexRefPtr& retainedSceneIndex,
    const SdfPath&                    primPath,
    const VtVec3fArray&               points,
    const VtVec2fArray&               uvs,
    const VtVec3fArray&               vertexColors,
    const VtIntArray&                 faceVertexCount,
    const VtIntArray&                 faceVertexIndices,
    const SdfPath&                    materialPath)
{
    using _Vec3ArrayDs = HdRetainedTypedSampledDataSource<VtVec3fArray>;
    using _IntArrayDs = HdRetainedTypedSampledDataSource<VtIntArray>;
    using _Vec2ArrayDs = HdRetainedTypedSampledDataSource<VtVec2fArray>;

    const _IntArrayDs::Handle fvcDs = _IntArrayDs::New(faceVertexCount);
    const _IntArrayDs::Handle fviDs = _IntArrayDs::New(faceVertexIndices);

    const HdContainerDataSourceHandle meshDs
        = HdMeshSchema::Builder()
              .SetTopology(HdMeshTopologySchema::Builder()
                               .SetFaceVertexCounts(fvcDs)
                               .SetFaceVertexIndices(fviDs)
                               .Build())
              .SetDoubleSided(
                  HdRetainedTypedSampledDataSource<bool>::New(true)) // Make the mesh double sided
              .Build();

    const HdContainerDataSourceHandle primvarsDs = HdRetainedContainerDataSource::New(

        // Create the vertices positions
        HdPrimvarsSchemaTokens->points,
        HdPrimvarSchema::Builder()
            .SetPrimvarValue(_Vec3ArrayDs::New(points))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
            .SetRole(HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->point))
            .Build(),

        // Create the vertex colors
        HdTokens->displayColor,
        HdPrimvarSchema::Builder()
            .SetIndexedPrimvarValue(_Vec3ArrayDs::New(vertexColors))
            .SetIndices(_IntArrayDs::New(faceVertexIndices))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
            .SetRole(
                HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->color)) // vertex color
            .Build(),

        // Create the UVs
        _tokens->st,
        HdPrimvarSchema::Builder()
            .SetIndexedPrimvarValue(_Vec2ArrayDs::New(uvs))
            .SetIndices(_IntArrayDs::New(faceVertexIndices))
            .SetInterpolation(
                HdPrimvarSchema::BuildInterpolationDataSource(HdPrimvarSchemaTokens->vertex))
            .SetRole(
                HdPrimvarSchema::BuildRoleDataSource(HdPrimvarSchemaTokens->color)) // vertex color
            .Build());

    GfMatrix4d transform;
    transform.SetIdentity();

    HdDataSourceBaseHandle const materialBindingSources[]
        = { HdMaterialBindingSchema::Builder()
                .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(materialPath))
                .Build() };

    // Create the primitive
    HdRetainedSceneIndex::AddedPrimEntry addedPrim;
    addedPrim.primPath = primPath;
    addedPrim.primType = HdPrimTypeTokens->mesh;
    addedPrim.dataSource = HdRetainedContainerDataSource::New(
        // Create a matrix
        HdXformSchemaTokens->xform,
        HdXformSchema::Builder()
            .SetMatrix(HdRetainedTypedSampledDataSource<GfMatrix4d>::New(transform))
            .Build(),

        // Create an extent attribute to support the viewport bounding box display style,
        // if no extent attribute is added, it will not be displayed at all in bounding box display
        // style
        HdExtentSchemaTokens->extent,
        HdExtentSchema::Builder()
            .SetMin(HdRetainedTypedSampledDataSource<GfVec3d>::New(
                GfVec3d(corner1.x, corner1.y, corner1.z)))
            .SetMax(HdRetainedTypedSampledDataSource<GfVec3d>::New(
                GfVec3d(corner2.x, corner2.y, corner2.z)))
            .Build(),

        // Assign the material
        HdMaterialBindingsSchemaTokens->materialBindings,
        HdMaterialBindingsSchema::BuildRetained(
            TfArraySize(purposes), purposes, materialBindingSources),

        // create a mesh
        HdMeshSchemaTokens->mesh,
        meshDs,
        HdPrimvarsSchemaTokens->primvars,
        primvarsDs);

    // Add the prim in the retained scene index
    retainedSceneIndex->AddPrims({ addedPrim });
}

// Get the color attribute value which is a double3
void GetDouble3AttributeValue(double3& outVal, const MObject& node, const MObject& attr)
{
    MPlug plug(node, attr);
    if (plug.isNull()) {
        return;
    }

    MObject oDouble3;
    plug.getValue(oDouble3);

    MFnNumericData fnData(oDouble3);
    fnData.getData(outVal[0], outVal[1], outVal[2]);
}

//Class to hold a material data source
class _MaterialDataSource : public HdContainerDataSource
{
public:
    HD_DECLARE_DATASOURCE(_MaterialDataSource);

    // ------------------------------------------------------------------------
    // HdContainerDataSource implementations
    TfTokenVector          GetNames() override{
        TfTokenVector result;
        result.push_back(HdMaterialSchemaTokens->material);
        return result;
    }

    HdDataSourceBaseHandle Get(const TfToken& name) override{
        if (name == HdMaterialSchemaTokens->material) {
            return _GetMaterialDataSource();
        } else if (name == HdMaterialBindingsSchema::GetSchemaToken()) {
            return _GetMaterialBindingDataSource();
        }
        return nullptr;
    }

private:
    _MaterialDataSource(const SdfPath& id, VtValue materialContainer)
        : _id(id), _materialContainer(materialContainer) {}

    HdDataSourceBaseHandle _GetMaterialDataSource(){
        if (!_materialContainer.IsHolding<HdMaterialNetworkMap>()) {
            return nullptr;
        }

        HdMaterialNetworkMap hdNetworkMap = _materialContainer.UncheckedGet<HdMaterialNetworkMap>();
        HdContainerDataSourceHandle materialDS = nullptr;
        if (!_ConvertHdMaterialNetworkToHdDataSources(hdNetworkMap, &materialDS)) {
            return nullptr;
        }
        return materialDS;
    }

    HdDataSourceBaseHandle _GetMaterialBindingDataSource(){
        if (_id.IsEmpty()) {
            return nullptr;
        }
        
        HdDataSourceBaseHandle const materialBindingSources[]
            = { HdMaterialBindingSchema::Builder()
                    .SetPath(HdRetainedTypedSampledDataSource<SdfPath>::New(_id))
                    .Build() };

        return HdMaterialBindingsSchema::BuildRetained(
            TfArraySize(purposes), purposes, materialBindingSources);
    }

    SdfPath             _id;
    VtValue             _materialContainer;
};

HD_DECLARE_DATASOURCE_HANDLES(_MaterialDataSource);

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Maya node implementation 
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class MhCustomShaders : public MPxLocatorNode
{
public:
    MhCustomShaders() = default;
    ~MhCustomShaders() override;

    //Is called when the MObject has been constructed and is valid
    void    postConstructor() override;

    MStatus   		compute( const MPlug& plug, MDataBlock& data ) override;

    bool            isBounded() const override;
    MBoundingBox    boundingBox() const override;

    static  void *      creator();
    static  MStatus     initialize();

    void UpdateColorInShader(const double3& color);

    // Callback when the node is added to the model (create /
    // undo-delete)
    void addedToModelCb();
    // Callback when the node is removed from model (delete)
    void removedFromModelCb();

    //Attributes
    static MObject     mColor;

    static	MTypeId		id;
    static	MString		nodeClassification;

private:
    ///get the value of the color attribute, returned as a 3D Hydra vector 
    GfVec3f _GetColor() const;

    /// Create the Hydra material
    void _CreateAndAddMaterials();
    
    /// Update color in the material network parameters
    void _UpdateMaterialColor(const double3& color);

    ///Counter to make the hydra primitives unique
    static std::atomic_int _counter;

    //Path of the quad primitive
    SdfPath _quadPrimPath;
    
    /// path to be used in the retained hydra scene index for the main primitive
    SdfPath _materialPath;
    VtValue _materialContainer;
    HdMaterialNetworkMap _networkMap;

    ///Hydra retained scene index to add the primitives
    HdRetainedSceneIndexRefPtr  _retainedSceneIndex  {nullptr};

    ///To hold the afterOpenCallback Id to be able to react when a File Open has happened.
    MCallbackId                 _cbAfterOpenId = 0;
    ///To hold the attributeChangedCallback Id to be able to react when the 3D grid creation parameters attributes from this node change.
    MCallbackId                 _cbAttributeChangedId = 0;

    MCallbackId _nodeAddedToModelCbId{0};
    MCallbackId _nodeRemovedFromModelCbId{0};
};

namespace 
{
    //Callback when an attribute of the maya node changes
    void attributeChangedCallback(MNodeMessage::AttributeMessage msg, MPlug& plug, MPlug & otherPlug, void* customShadersData)
    {
        if (! customShadersData){
            return;
        }

        MhCustomShaders* customShadersNode = reinterpret_cast<MhCustomShaders*>(customShadersData);

        MPlug parentPlug = plug.parent();
        if (parentPlug == MhCustomShaders::mColor){
            auto dataHandle
                = parentPlug.asMDataHandle(); // Using parent plug as this is composed of 3 doubles
            const double3& color = dataHandle.asDouble3();
            customShadersNode->UpdateColorInShader(color);
        }else
        if(plug == MhCustomShaders::mColor){
            auto           dataHandle = plug.asMDataHandle();
            const double3& color = dataHandle.asDouble3();
            customShadersNode->UpdateColorInShader(color);
        }
    }

    void nodeAddedToModel(MObject& node, void* /* clientData */)
    {
        auto fpNode = reinterpret_cast<MhCustomShaders*>(MFnDagNode(node).userNode());
        if (!TF_VERIFY(fpNode)) {
            return;
        }

        fpNode->addedToModelCb();
    }

    void nodeRemovedFromModel(MObject& node, void* /* clientData */)
    {
        auto fpNode = reinterpret_cast<MhCustomShaders*>(MFnDagNode(node).userNode());
        if (!TF_VERIFY(fpNode)) {
            return;
        }

        fpNode->removedFromModelCb();
    }
}//end of anonymous namespace

//Static variables init
std::atomic_int MhCustomShaders::_counter {0};
MObject MhCustomShaders::mColor;
MTypeId MhCustomShaders::id( 0x58000995 );
MString	MhCustomShaders::nodeClassification("hydraAPIExample/geometry/mhCustomShadersNode");

namespace {
    //Callback after a File Open
    void afterOpenCallback (void *clientData) 
    {
        if (! clientData){
            return;
        }

        //Trigger a call to compute so that everything is initialized
        MhCustomShaders* customShadersNodeInstance = reinterpret_cast<MhCustomShaders*>(clientData);
        customShadersNodeInstance->addedToModelCb();
    }
}

void MhCustomShaders::postConstructor()
{
    //We have a valid MObject in this function
    _quadPrimPath = SdfPath(std::string("/FVP_CustomShadersNode_") + std::to_string(_counter));
    _counter++;
    
    _materialPath = SdfPath(std::string("/FVP_CustomShadersNode_Material_") + std::to_string(_counter));

    //Add a callback after a load scene
    _cbAfterOpenId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, afterOpenCallback, ((void*)this)) ;

    //Add the callback when an attribute of this node changes
    MObject obj = thisMObject();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(obj, attributeChangedCallback, ((void*)this));

    _retainedSceneIndex = HdRetainedSceneIndex::New();

    _CreateAndAddMaterials();
    _CreateAndAddPrim(_retainedSceneIndex, _quadPrimPath, gPrimPoints, gPrimUVs, gPrimVertexColors, gPrimFaceVertexCounts, gPrimFaceVertexIndices, _materialPath);
    
    _nodeAddedToModelCbId = MModelMessage::addNodeAddedToModelCallback(obj, nodeAddedToModel);
    _nodeRemovedFromModelCbId = MModelMessage::addNodeRemovedFromModelCallback(obj, nodeRemovedFromModel);
}

MhCustomShaders::~MhCustomShaders()
{
    _retainedSceneIndex->RemovePrims({ { _quadPrimPath, _materialPath} });//Remove the quad and the material

    //Remove the callbacks
    for(auto cbId : {_cbAfterOpenId, _cbAttributeChangedId, _nodeAddedToModelCbId, _nodeRemovedFromModelCbId}) {
        if (cbId) {
            CHECK_MSTATUS(MMessage::removeCallback(cbId));
        }
    }
    
    //Remove our retained scene index from hydra
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeViewportDataProducerSceneIndex(_retainedSceneIndex, pxr::FvpViewportAPITokens->allViewports);
}

//Create the Hydra materials in the retained scene index
void MhCustomShaders::_CreateAndAddMaterials() 
{ 
    //Get the value of color attribute
    const GfVec3f color  = _GetColor();
    _materialContainer = _CreateHydraCustomBasicLightingMaterial(_networkMap, _materialPath, color);

    //Add the material to the retained scene index
    auto customColorMaterialDataSource = _MaterialDataSource::New(_materialPath, _materialContainer);
    _retainedSceneIndex->AddPrims({ { _materialPath,
                 HdPrimTypeTokens->material,
                 customColorMaterialDataSource } });
}

// Retrieve value of the color attribute from the maya node
GfVec3f MhCustomShaders::_GetColor() const
{
    const MObject obj = thisMObject();
    MPlug plug(obj, MhCustomShaders::mColor);
    if (!plug.isNull())
    {
        double3 color;
        GetDouble3AttributeValue(color, obj, MhCustomShaders::mColor);
        return {(float)color[0], (float)color[1], (float)color[2]};
    }

    return GfVec3f(0.f,0.f,1.f);
}

MStatus MhCustomShaders::compute( const MPlug& plug, MDataBlock& dataBlock)
{
    return MS::kSuccess;
}

bool MhCustomShaders::isBounded() const
{
    return true;
}

MBoundingBox MhCustomShaders::boundingBox() const
{
    //corner1 and 2 are the bounding box corner of our Hydra quad geometry visible only under a Hydra viewport
    return MBoundingBox( corner1 , corner2 );
}

void* MhCustomShaders::creator()
{
    static const MString errorString("You need to load the mayaHydra plugin before creating this node.");

    int	isMayaHydraLoaded = false;
    // Validate that the mayaHydra plugin is loaded.
    MGlobal::executeCommand( "pluginInfo -query -loaded mayaHydra", isMayaHydraLoaded );
    if( ! isMayaHydraLoaded){
        MGlobal::displayError(errorString);	    
        return nullptr;
    }

    return new MhCustomShaders();
}

void MhCustomShaders::addedToModelCb()
{
    static const SdfPath noPrefix = SdfPath::AbsoluteRootPath();

    //Add the callback when an attribute of this node changes
    MObject obj = thisMObject();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(obj, attributeChangedCallback, ((void*)this));

    //Data producer scene index interface is used to add the retained scene index to all viewports with all render delegates
    auto& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.addDataProducerSceneIndex(_retainedSceneIndex, noPrefix, (void*)&obj, FvpViewportAPITokens->allViewports,FvpViewportAPITokens->allRenderers);
}

void MhCustomShaders::removedFromModelCb()
{
    //Remove the callback
    if (_cbAttributeChangedId){
        CHECK_MSTATUS(MMessage::removeCallback(_cbAttributeChangedId));
        _cbAttributeChangedId = 0;
    }

    //Remove the data producer scene index.
    auto& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeViewportDataProducerSceneIndex(_retainedSceneIndex, pxr::FvpViewportAPITokens->allViewports);
}

void MhCustomShaders::_UpdateMaterialColor(const double3& color)
{
    //Look in the material network which material node holds the _tokens->FVP_CustomColor parameter name
    HdMaterialNetwork& network  = _networkMap.map[HdMaterialTerminalTokens->surface];
    const int       numNodes    = (int)network.nodes.size();
    for (int i = 0; i < numNodes; ++i) {
        HdMaterialNode& node = network.nodes[i];
        const auto      it = node.parameters.find(_tokens->FVP_CustomColor);
        if (it != node.parameters.end()) {
            //Found it, update the parameter value in the node
            node.parameters[_tokens->FVP_CustomColor] = VtValue(GfVec3f(color[0], color[1], color[2]));
            break;
        }
    }

    //Update our container with the updated network
    _materialContainer = VtValue(_networkMap);
}

void MhCustomShaders::UpdateColorInShader(const double3& color)
{
    static const HdDataSourceLocatorSet locators(
        HdMaterialSchema::GetDefaultLocator().Append(HdMaterialSchemaTokens->material));

    _UpdateMaterialColor(color);

     /*These are attempts in doing updates but none of them worked
     HdDataSourceLocator locator(
        HdMaterialNetworkSchemaTokens->nodes,
        _tokens->FVP_CustomBasicLightingShader,
        HdMaterialNodeSchemaTokens->parameters,
        _tokens->FVP_CustomColor);

    //Dirty the material so it gets updated in Hydra
    //_retainedSceneIndex->DirtyPrims({ {_materialPath, HdMaterialSchema::GetDefaultLocator()} });
    //_retainedSceneIndex->DirtyPrims({ {_materialPath, locator} });
    */

    //So, to update, remove the material prim and add it again
    _retainedSceneIndex->RemovePrims({ {_materialPath} });
    auto customColorMaterialDataSource
        = _MaterialDataSource::New(_materialPath, _materialContainer);
    _retainedSceneIndex->AddPrims(
        { { _materialPath, HdPrimTypeTokens->material, customColorMaterialDataSource } });
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

MStatus MhCustomShaders::initialize()
{
    MFnUnitAttribute    unitFn;
    MFnNumericAttribute nAttr;

    //Create a color attribute for the custom shader color parameter
    mColor = nAttr.create("color", "col", MFnNumericData::k3Double, 1.0);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS ( nAttr.setDefault(0.0, 1.0, 0.0) );
    CHECK_MSTATUS ( addAttribute(mColor));
    
    return MS::kSuccess;
}

MStatus initializePlugin( MObject obj )
{
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "2025.0", "Any");
    
    MStatus   status;
    status = plugin.registerNode(
                "MhCustomShaders",
                MhCustomShaders::id,
                &MhCustomShaders::creator,
                &MhCustomShaders::initialize,
                MPxNode::kLocatorNode,
                &MhCustomShaders::nodeClassification);
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

    status = plugin.deregisterNode( MhCustomShaders::id );
    if (!status) {
        status.perror("deregisterNode");
        return status;
    }
    return status;
}