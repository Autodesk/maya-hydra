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
#include <maya/MEventMessage.h>
#include <maya/MMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MModelMessage.h>
#include <maya/MDagPath.h>
#include <maya/MSelectionList.h>
#include <maya/MPointArray.h>
#include <maya/MEvaluationNode.h>
#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MAnimControl.h>
#include <maya/MTime.h>

// MayaHydra headers.
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/pick/mhPickHandler.h>
#include <mayaHydraLib/pick/mhPickHit.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <ufeExtensions/Global.h>

//Flow viewport headers
#include <flowViewport/API/fvpVersionInterface.h>
#include <flowViewport/API/fvpDataProducerSceneIndexInterface.h>
#include <flowViewport/selection/fvpPrefixPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>
#include <flowViewport/fvpPurposeRenderTagsForPasses.h>
#include <flowViewport/fvpDirtyNotifier.h>

//Hydra headers
#include <pxr/base/vt/array.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/dataSource.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/extentSchema.h>
#include <pxr/imaging/hd/purposeSchema.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {
void nodeAddedToModel(MObject& node, void* clientData);
void nodeRemovedFromModel(MObject& node, void* clientData);

// Sampled data source that reads live values from caller-owned storage so Hydra
// re-pulls updated matrices/colors after targeted dirty locators are emitted.
//
// Lifetime (plain T* — caller retains ownership):
// - Storage is a field on FootPrintMeshPrimState (_solePrimState / _heelPrimState
//   on MhFootPrint); New() stores the address of that member, not a temporary.
// - Each data source is owned by a prim entry in _retainedSceneIndex for the
//   node's lifetime.
// - Hydra may read through _value only while _retainedSceneIndex is registered
//   (addedToModelCb … removedFromModelCb / ~MhFootPrint); both unregister paths
//   call removeDataProducerSceneIndex before the node or its state is destroyed.
// - ~MhFootPrint() also Reset()s _retainedSceneIndex so data sources (and their
//   raw pointers) are dropped before _solePrimState/_heelPrimState are destroyed.
//   Updates mutate those fields in place so the stored pointer stays valid.
template<typename T>
class _MutableTypedSampledDataSource : public HdTypedSampledDataSource<T>
{
public:
    using Handle = typename HdTypedSampledDataSource<T>::Handle;

    static Handle New(T* value) { return Handle(new _MutableTypedSampledDataSource(value)); }

    // Return the current live value.
    T GetTypedValue(HdSampledDataSource::Time /*shutterOffset*/) override { return *_value; }

    // Wrap the stored value as a VtValue for Hydra data source queries.
    VtValue GetValue(HdSampledDataSource::Time shutterOffset) override
    {
        return VtValue(GetTypedValue(shutterOffset));
    }

    // Report a single uniform sample so Hydra re-reads after dirty locators.
    bool GetContributingSampleTimesForInterval(
        HdSampledDataSource::Time /*startTime*/,
        HdSampledDataSource::Time /*endTime*/,
        std::vector<HdSampledDataSource::Time>* /*outSampleTimes*/) override
    {
        // Uniform value for the frame — match HdRetainedTypedSampledDataSource so
        // Hydra calls GetValue(0) after dirty locators instead of caching samples.
        return false;
    }

private:
    explicit _MutableTypedSampledDataSource(T* value) : _value(value) {}

    T* _value;
};

// Live Hydra mesh state for one footprint part (sole or heel). Holds the mutable
// values that our custom data sources read on re-pull after dirty locators are
// emitted; topology (points, face indices) stays static in AddPrims.
//
// Lifetime: _MutableTypedSampledDataSource instances built over these fields
// hold raw pointers into the owning MhFootPrint's _solePrimState/_heelPrimState
// (see below). Those data sources must stop being queried before this struct
// is destroyed, which is why ~MhFootPrint() unregisters _retainedSceneIndex from
// Hydra and Reset()s it before member teardown.
struct FootPrintMeshPrimState
{
    GfMatrix4d    xform;
    GfVec3d       extentMin;
    GfVec3d       extentMax;
    VtVec3fArray  displayColors;
    TfToken       purpose;
};

// Pick handler for the footprint node.
class FootPrintPickHandler : public MayaHydra::PickHandler {
public:

    FootPrintPickHandler(MObject& footPrintObj) : _footPrintObj(footPrintObj) {}

    // Map a Hydra pick hit to the owning Maya footprint shape.
    bool handlePickHit(
        const Input& pickInput, Output& pickOutput
    ) const override
    {
        // Foot print parts are not selectable individually: only the complete
        // Maya shape object is selectable.
        //
        // Conceptually we could append a picked Maya object either to the
        // classic Maya MSelectionList selection or to the "MayaSelectTool"
        // named UFE selection (which provides input for the global selection).
        // However, the Maya select context filters out Maya items added to the
        // "MayaSelectTool" named UFE selection, so add to the MSelectionList.
        MDagPath dagPath;
        TF_AXIOM(MDagPath::getAPathTo(_footPrintObj, dagPath) == MS::kSuccess);
        pickOutput.mayaSelection.add(dagPath);
        const auto& wsPt = pickInput.pickHit.hdxPickHit.worldSpaceHitPoint;
        pickOutput.mayaWorldSpaceHitPts.append(wsPt[0], wsPt[1], wsPt[2]);

        return true;
    }

private:

    MObject _footPrintObj;
};

}

//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
// Node implementation with Hydra scene index
//---------------------------------------------------------------------------
//---------------------------------------------------------------------------
class MhFootPrint : public MPxLocatorNode
{
public:
    MhFootPrint() = default;
    ~MhFootPrint() override;

    //Is called when the MObject has been constructed and is valid
    void    postConstructor() override;

    MStatus compute( const MPlug& plug, MDataBlock& data ) override;

    // Called before this node is evaluated by Evaluation Manager (playback / driven attrs).
    MStatus preEvaluation(
        const MDGContext& context,
        const MEvaluationNode& evaluationNode) override;

    bool            isBounded() const override;
    MBoundingBox    boundingBox() const override;

    /// Update Hydra mesh prims from Maya attributes via targeted dirty locators.
    /// When \p changedPlug is set, only that footprint attribute is synced so
    /// unrelated attrs (e.g. keyed size) are not re-read in the wrong MDG context.
    void syncHydraFromNode(const MPlug* changedPlug = nullptr);
    /// Same as syncHydraFromNode but reads attrs at the current timeline time.
    void syncHydraFromNodeAtCurrentTime(const MPlug* changedPlug = nullptr);
    /// Read attrs from normal DG context after kAttributeSet on a driven plug.
    /// Uses fsNormal so a setAttr at the current frame without a new key reflects
    /// the edited datablock value instead of the anim-curve evaluation.
    void syncHydraFromNodeFromAttributeSet(const MPlug* changedPlug);
    /// Sync display attrs when the timeline frame changes (scrub / playback).
    void onTimeChanged();
    void _UpdateTimeChangedCallback();
    void _UpdateCachedSyncTimeFromTimeline();
    /// True when size, color, or purpose is connected (keys or DG-driven by another node).
    bool _HasAnyConnectedTimelineAttribute() const;
    
    static  void *      creator();
    static  MStatus     initialize();

    // Callback when the footprint node is added to the model (create /
    // undo-delete)
    void addedToModelCb();
    // Callback when the footprint node is removed from model (delete)
    void removedFromModelCb();

    Ufe::Path getUfePath() const;

    static bool IsFootPrintAttributePlug(const MPlug& plug);
    static bool IsSizeAttributePlug(const MPlug& plug);
    static bool IsColorAttributePlug(const MPlug& plug);
    static bool IsPurposeAttributePlug(const MPlug& plug);

    //Attributes
    static MObject     mSize;
    static MObject     mWorldS;
    static MObject     mColor;
    static MObject     mDrawAsSecondaryGraphics;

    static	MTypeId		id;
    static	MString		nodeClassification;

private:
    ///get the value of the size attribute in centimeters
    float _GetSizeInCentimeters() const;
    ///get the value of the color attribute, returned as a 3D Hydra vector 
    GfVec3f _GetColor() const;
    /// get the value of the DrawAsSecondaryGraphics attribute, which is a boolean, this is used so that when the mesh is drawn as a secondary graphics
    // it appears in the secondary graphics pass of maya hydra.
    // This is used as an example to show how to set primitives as secondary graphics
    bool _GetDrawAsSecondaryGraphics() const;
    TfToken _GetPurposeToken() const;

    void _InitPrimState(
        FootPrintMeshPrimState& state,
        float size,
        const GfVec3f& displayColor,
        const TfToken& purpose) const;

    void _UpdatePrimScale(FootPrintMeshPrimState& state, float size) const;

    ///Create the Hydra foot print primitives
    void _CreateAndAddFootPrintPrimitives();
    void _AddFootPrintPrim(
        const SdfPath&              primPath,
        FootPrintMeshPrimState&     state,
        const VtArray<GfVec3f>&     points,
        const VtIntArray&           faceVertexCount,
        const VtIntArray&           faceVertexIndices);

    void _DirtyPrimChanges(
        const SdfPath& path,
        bool sizeChanged,
        bool colorChanged,
        bool purposeChanged);

    ///Counter to make the hydra primitives unique
    static std::atomic_int _counter;

    /// Sole path to be used in the retained hydra scene index for the sole primitive
    SdfPath                     _solePath;
    /// Heel path to be used in the retained hydra scene index for the heel primitive
    SdfPath                     _heelPath;

    /// Live mesh state read by mutable data sources in _retainedSceneIndex.
    FootPrintMeshPrimState      _solePrimState;
    FootPrintMeshPrimState      _heelPrimState;

    bool   _primsAdded = false;
    float  _cachedSize = -1.0f;
    GfVec3f _cachedColor { -1.0f, -1.0f, -1.0f };
    bool   _cachedDrawAsSecondaryGraphics = false;
    bool   _cachedSyncTimeValid = false;
    MTime  _cachedSyncTime;
    /// After kAttributeSet, EM also marks the plug dirty and calls preEvaluation()
    /// for the same frame. Skip that duplicate resync once — re-reading a still-
    /// connected plug in EM's context would evaluate through the connection and
    /// discard the setAttr override we already synced from fsNormal.
    bool   _sizeManualOverridePendingPreEval = false;
    bool   _colorManualOverridePendingPreEval = false;
    bool   _purposeManualOverridePendingPreEval = false;

    ///To hold the afterOpenCallback Id to be able to react when a File Open has happened.
    MCallbackId                 _cbAfterOpenId = 0;
    ///To hold the attributeChangedCallback Id to be able to react when the footprint creation parameters attributes from this node change.
    MCallbackId                 _cbAttributeChangedId = 0;
    /// timeChanged event: connected display attrs are not always re-evaluated during scrub.
    MCallbackId                 _cbTimeChangedId = 0;

    MCallbackId _nodeAddedToModelCbId{0};
    MCallbackId _nodeRemovedFromModelCbId{0};

    SdfPath _pathPrefix;
    Ufe::Path _appPath{};

    /// Hydra retained scene index holding the 2 foot print primitives.
    /// Teardown: removeDataProducerSceneIndex in removedFromModelCb() and
    /// ~MhFootPrint(), then Reset() in ~MhFootPrint() before member destruction.
    HdRetainedSceneIndexRefPtr  _retainedSceneIndex  {nullptr};
};

namespace 
{
    // Canonical footprint mesh data in local space (Y-up, unit-scale footprint).
    // solePoints: 21 vertices; heelPoints: 17 vertices. Size is applied via xform scale.
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
    
    // Sole: 19 triangle faces (fan from vertex 0); one faceVertexCount entry per face, each 3.
    static const VtIntArray soleFaceVertexCounts    =   {
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3
                                                    };

    // Heel: 15 triangle faces (fan from vertex 0); one faceVertexCount entry per face, each 3.
    static const VtIntArray heelFaceVertexCounts    =   {
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3,
                                                        3, 3, 3, 3, 3
                                                    };

    // Per-face vertex indices into solePoints (3 indices per triangle, 19 faces).
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

    // Per-face vertex indices into heelPoints (3 indices per triangle, 15 faces).
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

    // Axis-aligned bounds of the unit-scale footprint in local space (used for Maya boundingBox and Hydra extent).
    static const MPoint corner1( -0.17, 0.0, -0.7 );
    static const MPoint corner2( 0.17, 0.0, 0.3 );

    // True when the attribute message can change footprint display values.
    bool attributeMessageAffectsFootPrintAttributes(MNodeMessage::AttributeMessage msg)
    {
        const MNodeMessage::AttributeMessage mask = static_cast<MNodeMessage::AttributeMessage>(
            MNodeMessage::kAttributeSet | MNodeMessage::kAttributeRemoved
            | MNodeMessage::kAttributeAdded | MNodeMessage::kAttributeRenamed);
        return (msg & mask) != 0;
    }

    // Sync Hydra when a footprint attribute is set, added, removed, or renamed.
    void attributeChangedCallback(
        MNodeMessage::AttributeMessage msg,
        MPlug& plug,
        MPlug& /*otherPlug*/,
        void* footPrintData)
    {
        if (!footPrintData || !MhFootPrint::IsFootPrintAttributePlug(plug)) {
            return;
        }

        // kAttributeEval alone does not carry a user edit; skip it.
        if ((msg & MNodeMessage::kAttributeEval)
            && !(msg & MNodeMessage::kAttributeSet)) {
            return;
        }

        if (attributeMessageAffectsFootPrintAttributes(msg)) {
            auto* footPrint = reinterpret_cast<MhFootPrint*>(footPrintData);
            // kAttributeSet: read the plug from the datablock (fsNormal).
            // Other messages: read the plug at the current timeline time.
            if (msg & MNodeMessage::kAttributeSet) {
                footPrint->syncHydraFromNodeFromAttributeSet(&plug);
            } else {
                footPrint->syncHydraFromNodeAtCurrentTime(&plug);
            }
            // Keying/connect/disconnect may change whether we need timeChanged for scrub.
            footPrint->_UpdateTimeChangedCallback();
        }
    }

    // Forward Maya timeChanged events to the footprint node.
    void timeChangedCallback(void* clientData)
    {
        if (auto* footPrint = reinterpret_cast<MhFootPrint*>(clientData)) {
            footPrint->onTimeChanged();
        }
    }

// Register the footprint with Hydra when the node enters the scene.
void nodeAddedToModel(MObject& node, void* /* clientData */)
{
    auto fpNode = reinterpret_cast<MhFootPrint*>(MFnDagNode(node).userNode());
    if (!TF_VERIFY(fpNode)) {
        return;
    }

    fpNode->addedToModelCb();
}

// Unregister the footprint from Hydra when the node leaves the scene.
void nodeRemovedFromModel(MObject& node, void* /* clientData */)
{
    auto fpNode = reinterpret_cast<MhFootPrint*>(MFnDagNode(node).userNode());
    if (!TF_VERIFY(fpNode)) {
        return;
    }

    fpNode->removedFromModelCb();
}

}
//end of anonymous namespace

//Static variables init
std::atomic_int MhFootPrint::_counter {0};
MObject MhFootPrint::mSize;
MObject MhFootPrint::mColor;
MObject MhFootPrint::mDrawAsSecondaryGraphics;
MTypeId MhFootPrint::id( 0x58000994 );
MString	MhFootPrint::nodeClassification("hydraAPIExample/geometry/footPrint");
MObject MhFootPrint::mWorldS;

namespace {
    // Re-sync Hydra from saved datablock values after a scene file is opened.
    void afterOpenCallback (void *clientData)
    {
        if (! clientData){
            return;
        }

        auto* footPrint = reinterpret_cast<MhFootPrint*>(clientData);
        {
            MDGContextGuard guard(MDGContext::fsNormal);
            footPrint->syncHydraFromNode(/*changedPlug=*/nullptr);
        }
        footPrint->_UpdateCachedSyncTimeFromTimeline();
        footPrint->_UpdateTimeChangedCallback();
        // No need to call footPrint->addedToModelCb(), as reading the file will
        // add the node to the model.
    }
}

/* static */
// True when the plug is the size attribute.
bool MhFootPrint::IsSizeAttributePlug(const MPlug& plug)
{
    MStatus status;
    return plug.attribute(&status) == MhFootPrint::mSize && status;
}

/* static */
// True when the plug is the color attribute or one of its child plugs.
bool MhFootPrint::IsColorAttributePlug(const MPlug& plug)
{
    MStatus status;
    const MObject attr = plug.attribute(&status);
    if (!status) {
        return false;
    }
    if (attr == MhFootPrint::mColor) {
        return true;
    }
    const MPlug parentPlug = plug.parent();
    return parentPlug.attribute(&status) == MhFootPrint::mColor && status;
}

/* static */
// True when the plug is drawAsSecondaryGraphics, which maps to the Hydra
// purpose render tag (see _GetPurposeToken()).
bool MhFootPrint::IsPurposeAttributePlug(const MPlug& plug)
{
    MStatus status;
    return plug.attribute(&status) == MhFootPrint::mDrawAsSecondaryGraphics && status;
}

/* static */
// True when the plug drives footprint size, color, or purpose.
bool MhFootPrint::IsFootPrintAttributePlug(const MPlug& plug)
{
    return IsSizeAttributePlug(plug)
        || IsColorAttributePlug(plug)
        || IsPurposeAttributePlug(plug);
}

// Set up paths, callbacks, retained scene index, and initial Hydra prims.
void MhFootPrint::postConstructor()
{
    _solePath = SdfPath(std::string("/sole_") + std::to_string(_counter));
    _heelPath = SdfPath(std::string("/heel_") + std::to_string(_counter));
    _counter++;

    _cbAfterOpenId = MSceneMessage::addCallback(MSceneMessage::kAfterOpen, afterOpenCallback, ((void*)this));

    MObject obj = thisMObject();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(
        obj, attributeChangedCallback, ((void*)this));

    _retainedSceneIndex = HdRetainedSceneIndex::New();

    _CreateAndAddFootPrintPrimitives();

    _nodeAddedToModelCbId = MModelMessage::addNodeAddedToModelCallback(obj, nodeAddedToModel);
    _nodeRemovedFromModelCbId = MModelMessage::addNodeRemovedFromModelCallback(obj, nodeRemovedFromModel);
}

// Remove callbacks and unregister the retained scene index from Hydra.
MhFootPrint::~MhFootPrint()
{
    for (auto cbId : {_cbAfterOpenId, _cbAttributeChangedId, _cbTimeChangedId,
                      _nodeAddedToModelCbId, _nodeRemovedFromModelCbId}) {
        if (cbId) {
            CHECK_MSTATUS(MMessage::removeCallback(cbId));
        }
    }
    
    Fvp::DataProducerSceneIndexInterface& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeDataProducerSceneIndex(_retainedSceneIndex, PXR_NS::FvpViewportAPITokens->allRenderViews);
    // Drop data sources (and their raw pointers into _sole/_heelPrimState) before
    // member destruction; do not rely on declaration order.
    _retainedSceneIndex.Reset();
}

// Initialize sole/heel mesh state from size, color, and purpose.
void MhFootPrint::_InitPrimState(
    FootPrintMeshPrimState& state,
    float size,
    const GfVec3f& displayColor,
    const TfToken& purpose) const
{
    _UpdatePrimScale(state, size);
    state.displayColors = VtVec3fArray{ displayColor };
    state.purpose = purpose;
}

// Build one mesh prim and add it to the retained scene index.
void MhFootPrint::_AddFootPrintPrim(
    const SdfPath&              primPath,
    FootPrintMeshPrimState&     state,
    const VtArray<GfVec3f>&     points,
    const VtIntArray&           faceVertexCount,
    const VtIntArray&           faceVertexIndices)
{
    using _PointArrayDs = HdRetainedTypedSampledDataSource<VtArray<GfVec3f>>;
    using _IntArrayDs   = HdRetainedTypedSampledDataSource<VtIntArray>;

    const _IntArrayDs::Handle fvcDs = _IntArrayDs::New(faceVertexCount);
    const _IntArrayDs::Handle fviDs = _IntArrayDs::New(faceVertexIndices);

    const VtIntArray vertexColorArray(points.size(), 0);//Is an index in the vertex color array, 1 per vertex,but  we only have one same color for all verts (index 0)

    const HdContainerDataSourceHandle meshDs =
        HdMeshSchema::Builder()
            .SetTopology(
                HdMeshTopologySchema::Builder()
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
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->vertex))
                .SetRole(
                    HdPrimvarSchema::BuildRoleDataSource(
                        HdPrimvarSchemaTokens->point))
                .Build(),
            //Create the vertex colors
            HdTokens->displayColor,
            HdPrimvarSchema::Builder()
                .SetIndexedPrimvarValue(
                    _MutableTypedSampledDataSource<VtVec3fArray>::New(&state.displayColors))
                .SetIndices(_IntArrayDs::New(vertexColorArray))
                .SetInterpolation(
                    HdPrimvarSchema::BuildInterpolationDataSource(
                        HdPrimvarSchemaTokens->varying))
                .SetRole(
                    HdPrimvarSchema::BuildRoleDataSource(
                        HdPrimvarSchemaTokens->color))//vertex color
                .Build());

    //Create the primitive
    HdRetainedSceneIndex::AddedPrimEntry addedPrim;
    addedPrim.primPath   = primPath;
    addedPrim.primType   = HdPrimTypeTokens->mesh;
    addedPrim.dataSource = HdRetainedContainerDataSource::New(
        //Create a matrix
        HdXformSchemaTokens->xform,
        HdXformSchema::Builder()
            .SetMatrix(_MutableTypedSampledDataSource<GfMatrix4d>::New(&state.xform))
            .Build(),
        //Create an extent attribute to support the viewport bounding box display style, 
        //if no extent attribute is added, it will not be displayed at all in bounding box display style
        HdExtentSchemaTokens->extent,
        HdExtentSchema::Builder()
            .SetMin(_MutableTypedSampledDataSource<GfVec3d>::New(&state.extentMin))
            .SetMax(_MutableTypedSampledDataSource<GfVec3d>::New(&state.extentMax))
            .Build(),
        // Create a purpose render tag which is a way to specify how the primitive will be drawn.
        // If the drawAsSecondaryGraphics is true, it will be drawn in the secondary graphics
        // render pass.
        HdPurposeSchemaTokens->purpose,
        HdPurposeSchema::Builder()
            .SetPurpose(_MutableTypedSampledDataSource<TfToken>::New(&state.purpose))
            .Build(),
        //create a mesh
        HdMeshSchemaTokens->mesh,
        meshDs,
        HdPrimvarsSchemaTokens->primvars,
        primvarsDs);

    //Add the prim in the retained scene index
    _retainedSceneIndex->AddPrims({ addedPrim });
}

// Create sole and heel prims from current Maya attribute values.
void MhFootPrint::_CreateAndAddFootPrintPrimitives()
{
    const float fSize = _GetSizeInCentimeters();
    const GfVec3f displayColor = _GetColor();
    const TfToken purpose = _GetPurposeToken();

    _InitPrimState(_solePrimState, fSize, displayColor, purpose);
    _InitPrimState(_heelPrimState, fSize, displayColor, purpose);

    _AddFootPrintPrim(
        _solePath,
        _solePrimState,
        solePoints,
        soleFaceVertexCounts,
        soleFaceVertexIndices);

    _AddFootPrintPrim(
        _heelPath,
        _heelPrimState,
        heelPoints,
        heelFaceVertexCounts,
        heelFaceVertexIndices);

    _primsAdded = true;
    _cachedSize = fSize;
    _cachedColor = displayColor;
    _cachedDrawAsSecondaryGraphics = _GetDrawAsSecondaryGraphics();
    _UpdateCachedSyncTimeFromTimeline();
}

// Notify Hydra which parts of a prim changed.
void MhFootPrint::_DirtyPrimChanges(
    const SdfPath& path,
    bool sizeChanged,
    bool colorChanged,
    bool purposeChanged)
{
    Fvp::DirtyNotifier notifier(*_retainedSceneIndex, path);
    if (sizeChanged) {
        notifier.dirtyTransform().dirtyExtent();
    }
    if (colorChanged) {
        notifier.dirtyVertexColors();
    }
    if (purposeChanged) {
        notifier.dirtyPurpose();
    }
}

// Update xform scale and extent from the new size value.
void MhFootPrint::_UpdatePrimScale(FootPrintMeshPrimState& state, float size) const
{
    state.xform.SetIdentity();
    state.xform.SetScale(GfVec3d(size, size, size));
    state.extentMin = GfVec3d(corner1.x * size, corner1.y * size, corner1.z * size);
    state.extentMax = GfVec3d(corner2.x * size, corner2.y * size, corner2.z * size);
}

// Read Maya attrs, update cached prim state, and mark dirty locators so Hydra
// re-pulls size/color/purpose from the data sources.
void MhFootPrint::syncHydraFromNode(const MPlug* changedPlug)
{
    if (!_retainedSceneIndex || !_primsAdded) {
        return;
    }

    const bool syncSize = !changedPlug || IsSizeAttributePlug(*changedPlug);
    const bool syncColor = !changedPlug || IsColorAttributePlug(*changedPlug);
    const bool syncPurpose = !changedPlug || IsPurposeAttributePlug(*changedPlug);

    bool sizeChanged = false;
    bool colorChanged = false;
    bool purposeChanged = false;

    float fSize = _cachedSize;
    GfVec3f displayColor = _cachedColor;
    bool drawAsSecondaryGraphics = _cachedDrawAsSecondaryGraphics;
    TfToken purpose = _solePrimState.purpose;

    if (syncSize) {
        fSize = _GetSizeInCentimeters();
        sizeChanged = !GfIsClose(_cachedSize, fSize, 1e-6f);
    }
    if (syncColor) {
        displayColor = _GetColor();
        colorChanged = _cachedColor != displayColor;
    }
    if (syncPurpose) {
        drawAsSecondaryGraphics = _GetDrawAsSecondaryGraphics();
        purpose = _GetPurposeToken();
        purposeChanged = _cachedDrawAsSecondaryGraphics != drawAsSecondaryGraphics;
    }

    if (!sizeChanged && !colorChanged && !purposeChanged) {
        return;
    }

    if (sizeChanged) {
        _UpdatePrimScale(_solePrimState, fSize);
        _UpdatePrimScale(_heelPrimState, fSize);
        _cachedSize = fSize;
    }

    if (colorChanged) {
        _solePrimState.displayColors = VtVec3fArray{ displayColor };
        _heelPrimState.displayColors = VtVec3fArray{ displayColor };
        _cachedColor = displayColor;
    }

    if (purposeChanged) {
        _solePrimState.purpose = purpose;
        _heelPrimState.purpose = purpose;
        _cachedDrawAsSecondaryGraphics = drawAsSecondaryGraphics;
    }

    _DirtyPrimChanges(_solePath, sizeChanged, colorChanged, purposeChanged);
    _DirtyPrimChanges(_heelPath, sizeChanged, colorChanged, purposeChanged);
}

// Sync Hydra using attribute values at the current timeline time.
void MhFootPrint::syncHydraFromNodeAtCurrentTime(const MPlug* changedPlug)
{
    MDGContextGuard guard(MAnimControl::currentTime());
    syncHydraFromNode(changedPlug);
    _UpdateCachedSyncTimeFromTimeline();
}

// Sync Hydra using the plug value stored in the datablock (fsNormal).
void MhFootPrint::syncHydraFromNodeFromAttributeSet(const MPlug* changedPlug)
{
    MDGContextGuard guard(MDGContext::fsNormal);
    syncHydraFromNode(changedPlug);
    _UpdateCachedSyncTimeFromTimeline();

    // Setting a driven plug also marks it dirty for Evaluation Manager's own
    // bookkeeping, which triggers a preEvaluation call for the same frame right
    // after this one. Flag that we already have the authoritative value so that
    // call doesn't clobber it (see _sizeManualOverridePendingPreEval and friends).
    if (!changedPlug || IsSizeAttributePlug(*changedPlug)) {
        _sizeManualOverridePendingPreEval = true;
    }
    if (!changedPlug || IsColorAttributePlug(*changedPlug)) {
        _colorManualOverridePendingPreEval = true;
    }
    if (!changedPlug || IsPurposeAttributePlug(*changedPlug)) {
        _purposeManualOverridePendingPreEval = true;
    }
}

// True when size, color, or purpose is connected and may vary with the timeline
// (animation curves, driven keys, or DG connections from other time-varying nodes).
bool MhFootPrint::_HasAnyConnectedTimelineAttribute() const
{
    const MObject obj = thisMObject();
    const MPlug   sizePlug(obj, mSize);
    const MPlug   colorPlug(obj, mColor);
    const MPlug   purposePlug(obj, mDrawAsSecondaryGraphics);
    return MayaHydra::PlugOrChildIsConnected(sizePlug)
        || MayaHydra::PlugOrChildIsConnected(colorPlug)
        || MayaHydra::PlugOrChildIsConnected(purposePlug);
}

// Sync connected display attrs when the timeline frame changes.
void MhFootPrint::onTimeChanged()
{
    if (!_retainedSceneIndex || !_primsAdded) {
        return;
    }

    if (!_HasAnyConnectedTimelineAttribute()) {
        return;
    }

    const MTime currentTime = MAnimControl::currentTime();
    if (_cachedSyncTimeValid && _cachedSyncTime == currentTime) {
        return;
    }

    syncHydraFromNodeAtCurrentTime(/*changedPlug=*/nullptr);
}

// Register timeChanged only while display attrs are connected to time-varying
// sources. Scrubbing does not always dirty this locator or fire attributeChanged,
// so we re-sync Hydra from MAnimControl::currentTime() on each frame change.
void MhFootPrint::_UpdateTimeChangedCallback()
{
    MStatus status;
    const bool needsCallback = _HasAnyConnectedTimelineAttribute();

    if (needsCallback && _cbTimeChangedId == 0) {
        _cbTimeChangedId = MEventMessage::addEventCallback(
            "timeChanged", timeChangedCallback, this, &status);
        if (!status) {
            _cbTimeChangedId = 0;
        }
    } else if (!needsCallback && _cbTimeChangedId != 0) {
        MMessage::removeCallback(_cbTimeChangedId);
        _cbTimeChangedId = 0;
    }
}

// Store the timeline time used for the last Hydra sync.
void MhFootPrint::_UpdateCachedSyncTimeFromTimeline()
{
    _cachedSyncTime = MAnimControl::currentTime();
    _cachedSyncTimeValid = true;
}

// Read the size attribute in centimeters.
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

// Read the drawAsSecondaryGraphics attribute.
bool MhFootPrint::_GetDrawAsSecondaryGraphics() const
{
    const MObject obj = thisMObject();
    MPlug plug(obj, MhFootPrint::mDrawAsSecondaryGraphics);
    if (!plug.isNull()) {
        bool val = false;
        if (plug.getValue(val)) {
            return val;
        }
    }

    return false;
}

// Map drawAsSecondaryGraphics to the Hydra purpose render tag.
TfToken MhFootPrint::_GetPurposeToken() const
{
    return _GetDrawAsSecondaryGraphics()
        ? Fvp::secondaryGraphicsRenderTagToken
        : HdRenderTagTokens->geometry;
}

// Read the color attribute as a GfVec3f.
GfVec3f MhFootPrint::_GetColor() const
{
    return MayaHydra::GetGfVec3fAttributeValue(
        thisMObject(), MhFootPrint::mColor, GfVec3f(0.f, 0.f, 1.f));
}

// Sync Hydra before EM evaluation, but only for contexts matching the actual
// current timeline time (see the early-out below for why).
MStatus MhFootPrint::preEvaluation(
    const MDGContext& context,
    const MEvaluationNode& evaluationNode)
{
    // preEvaluation can be invoked for MDGContexts that do not correspond to
    // the timeline's actual current time, e.g. Maya internally evaluating
    // other frames while recomputing anim curve tangents when a new key is
    // added. Treating such a call as a real "current time changed" event would
    // read size/color/purpose at that other frame, update cached state, and
    // mark dirty locators, clobbering the value actually being displayed
    // (regression: setting a key at the current frame with the already-correct
    // value could reset the displayed size back to a value keyed at another
    // frame). So bail out early for any non-normal context whose time isn't
    // the current time.
    const MTime currentTime = MAnimControl::currentTime();
    MStatus contextStatus;
    if (!context.isNormal(&contextStatus) && contextStatus) {
        MTime contextTime;
        if (context.getTime(contextTime) == MS::kSuccess && contextTime != currentTime) {
            return MS::kSuccess;
        }
    }

    MDGContextGuard guard(context);

    const MObject obj = thisMObject();
    const MPlug   sizePlug(obj, mSize);
    const MPlug   colorPlug(obj, mColor);
    const MPlug   purposePlug(obj, mDrawAsSecondaryGraphics);

    MStatus status;
    const bool timeChanged =
        !_cachedSyncTimeValid || (_cachedSyncTime != currentTime);
    // Re-sync a dirty plug unless we already synced it from a literal setAttr
    // value moments ago (see syncHydraFromNodeFromAttributeSet): re-reading a
    // still-connected plug here would evaluate through the connection and
    // discard that override.
    auto resyncIfNotAlreadyOverridden =
        [this](const MPlug& plug, bool& overridePending) {
            if (overridePending) {
                overridePending = false;
            } else {
                syncHydraFromNode(&plug);
            }
        };

    if (timeChanged) {
        _sizeManualOverridePendingPreEval = false;
        _colorManualOverridePendingPreEval = false;
        _purposeManualOverridePendingPreEval = false;
        syncHydraFromNode(/*changedPlug=*/nullptr);
        _cachedSyncTime = currentTime;
        _cachedSyncTimeValid = true;
    } else {
        if (evaluationNode.dirtyPlugExists(mSize, &status) && status) {
            resyncIfNotAlreadyOverridden(sizePlug, _sizeManualOverridePendingPreEval);
        }
        if (evaluationNode.dirtyPlugExists(mColor, &status) && status) {
            resyncIfNotAlreadyOverridden(colorPlug, _colorManualOverridePendingPreEval);
        }
        if (evaluationNode.dirtyPlugExists(mDrawAsSecondaryGraphics, &status) && status) {
            resyncIfNotAlreadyOverridden(purposePlug, _purposeManualOverridePendingPreEval);
        }
    }

    return MS::kSuccess;
}

// Handle worldS evaluation and keep Hydra scale in sync with size.
MStatus MhFootPrint::compute( const MPlug& plug, MDataBlock& dataBlock)
{
    if (plug == mWorldS) 
    {
        // Read size from this compute's own datablock rather than re-querying the
        // plug through a separate current-time MDGContext: building a distinct
        // MDGContext (even one for the same MTime) forces Maya to re-evaluate the
        // plug through any incoming connection (e.g. an animCurve), discarding a
        // setAttr override made on the driven plug at the current frame before a
        // new key is set. The datablock we were handed already reflects the
        // correct, up-to-date value for this evaluation.
        if (_retainedSceneIndex && _primsAdded) {
            const MDataHandle sizeHandle = dataBlock.inputValue(mSize);
            const float fSize = static_cast<float>(sizeHandle.asDistance().asCentimeters());
            if (!GfIsClose(_cachedSize, fSize, 1e-6f)) {
                _UpdatePrimScale(_solePrimState, fSize);
                _UpdatePrimScale(_heelPrimState, fSize);
                _cachedSize = fSize;
                _DirtyPrimChanges(
                    _solePath,
                    /*sizeChanged=*/true, /*colorChanged=*/false, /*purposeChanged=*/false);
                _DirtyPrimChanges(
                    _heelPath,
                    /*sizeChanged=*/true, /*colorChanged=*/false, /*purposeChanged=*/false);
            }
            _UpdateCachedSyncTimeFromTimeline();
        }

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

// Footprint geometry always has a bounded extent.
bool MhFootPrint::isBounded() const
{
    return true;
}

// Return the Maya bounding box scaled by the current size.
MBoundingBox MhFootPrint::boundingBox() const
{
    MDGContextGuard guard(MAnimControl::currentTime());
    const double multiplier = _GetSizeInCentimeters();
    return MBoundingBox( corner1 * multiplier, corner2 * multiplier);//corner1 and 2 are the bounding box corner of our geometry
}

// Factory: create the node when the mayaHydra plugin is loaded.
void* MhFootPrint::creator()
{
    static const MString errorString("You need to load the mayaHydra plugin before creating this node.");

    int	isMayaHydraLoaded = false;
    // Validate that the mayaHydra plugin is loaded.
    MGlobal::executeCommand( "pluginInfo -query -loaded mayaHydra", isMayaHydraLoaded );
    if( ! isMayaHydraLoaded){
        MGlobal::displayError(errorString);	    
        return nullptr;
    }

    return new MhFootPrint();
}

// Return the UFE path for this footprint shape.
Ufe::Path MhFootPrint::getUfePath() const
{
    MDagPath dagPath;
    TF_AXIOM(MDagPath::getAPathTo(thisMObject(), dagPath) == MS::kSuccess);
    return Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath));
}

// Publish the retained scene index, pick handler, and path mapper.
void MhFootPrint::addedToModelCb()
{
    _pathPrefix = SdfPath(TfStringPrintf("/MhFootPrint_%p", this));

    MObject obj = thisMObject();

    //Data producer scene index interface is used to add the retained scene index to all render views with all render delegates
    auto& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.addDataProducerSceneIndex(_retainedSceneIndex, _pathPrefix, (void*)&obj);

    // Register a pick handler for our prefix with the pick handler registry.
    auto pickHandler = std::make_shared<FootPrintPickHandler>(obj);
    TF_AXIOM(MayaHydra::PickHandlerRegistry::Instance().Register(_pathPrefix, pickHandler));

    // Register a path mapper to map application UFE paths to scene index paths,
    // for selection highlighting.
    _appPath = getUfePath();
    auto pathMapper = std::make_shared<Fvp::PrefixPathMapper>(_appPath, _pathPrefix);
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Register(_appPath, pathMapper));

    {
        syncHydraFromNodeAtCurrentTime();
    }

    _UpdateTimeChangedCallback();
}

// Unpublish Hydra resources when the node is removed from the scene.
void MhFootPrint::removedFromModelCb()
{
    if (_cbTimeChangedId != 0) {
        MMessage::removeCallback(_cbTimeChangedId);
        _cbTimeChangedId = 0;
    }

    // Unregister our path mapper.  Use stored UFE path, as at this point
    // our locator node is no longer in the Maya scene, so we cannot obtain
    // an MDagPath for it.
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Unregister(_appPath));

    // Unregister our pick handler.
    TF_AXIOM(MayaHydra::PickHandlerRegistry::Instance().Unregister(_pathPrefix));

    //Remove the data producer scene index.
    auto& dataProducerSceneIndexInterface = Fvp::DataProducerSceneIndexInterface::get();
    dataProducerSceneIndexInterface.removeDataProducerSceneIndex(_retainedSceneIndex, PXR_NS::FvpViewportAPITokens->allRenderViews);
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

// Define footprint node attributes and DG relationships.
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

    mDrawAsSecondaryGraphics = nAttr.create("drawAsSecondaryGraphics", "dag", MFnNumericData::kBoolean);
    MAKE_INPUT(nAttr);
    CHECK_MSTATUS(nAttr.setDefault(false));

    CHECK_MSTATUS ( addAttribute(mSize) );
    CHECK_MSTATUS ( addAttribute(mColor));
    CHECK_MSTATUS ( addAttribute(mDrawAsSecondaryGraphics));
    CHECK_MSTATUS ( addAttribute(mWorldS));
    
    CHECK_MSTATUS ( attributeAffects(mSize, mWorldS));
    return MS::kSuccess;
}

// Register the MhFootPrint node with Maya.
MStatus initializePlugin( MObject obj )
{
    MFnPlugin plugin( obj, PLUGIN_COMPANY, "2025.0", "Any");
    
    MStatus   status;
    status = plugin.registerNode(
                "MhFootPrint",
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

// Deregister the MhFootPrint node from Maya.
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

