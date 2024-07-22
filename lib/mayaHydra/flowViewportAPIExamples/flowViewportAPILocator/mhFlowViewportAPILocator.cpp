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
#include <flowViewport/selection/fvpPrefixPathMapper.h>
#include <flowViewport/selection/fvpPathMapperRegistry.h>

//Maya Hydra headers
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mayaHydraLibInterface.h>
#include <mayaHydraLib/pick/mhPickHandler.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <ufeExtensions/Global.h>
#include <ufeExtensions/cvtTypeUtils.h>

//Maya headers
#include <maya/MPxLocatorNode.h>
#include <maya/MFnPlugin.h>
#include <maya/MFnTransform.h>
#include <maya/MMatrix.h>
#include <maya/MFnMatrixData.h>
#include <maya/MFnTypedAttribute.h>
#include <maya/MFnNumericAttribute.h>
#include <maya/MFnMatrixAttribute.h>
#include <maya/MFnCompoundAttribute.h>
#include <maya/MDagPath.h>
#include <maya/MNodeMessage.h>
#include <maya/MSceneMessage.h>
#include <maya/MObjectHandle.h>
#include <maya/MGlobal.h>
#include <maya/MFnDagNode.h>
#include <maya/MModelMessage.h>

#include <pxr/imaging/hdx/pickTask.h>

#include <ufe/rtid.h>
#include <ufe/runTimeMgr.h>
#include <ufe/path.h>
#include <ufe/pathString.h>

#include <sstream>
#include <regex>

//We use a locator node to deal with creating and filtering hydra primitives as an example.
//But you could use another kind of maya plugin.

/*To create an instance of this node in maya, please use the following MEL command :
createNode("MhFlowViewportAPILocator")
*/

PXR_NAMESPACE_USING_DIRECTIVE

///Maya Locator node subclass to create filtering and data producer scene indices example, to be used with the flow viewport API.
class MhFlowViewportAPILocator : public MPxLocatorNode
{
public:
    using TransformedCubes = std::map<std::string, GfVec3d>;
    using HiddenCubes      = std::set<std::string>;

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
    static MObject mCubeInitialTransform;
    static MObject mCubeColor;
    static MObject mCubeOpacity;
    static MObject mCubesUseInstancing;
    static MObject mCubesDeltaTrans;
    static MObject mHiddenCubes;
    static MObject mCubeTranslateX;
    static MObject mCubeTranslateY;
    static MObject mCubeTranslateZ;
    static MObject mCubeTranslate;
    static MObject mTransformedCubeName;
    static MObject mTransformedCubes;
    
    ///3D Grid of cube mesh primitives creation parameters for the data producer scene index
    Fvp::DataProducerSceneIndexExample::CubeGridCreationParams  _cubeGridParams;
    ///_hydraViewportDataProducerSceneIndexExample is what will inject the 3D grid of Hydra cube mesh primitives into the viewport
    Fvp::DataProducerSceneIndexExample                          _hydraViewportDataProducerSceneIndexExample;

    // Callback when the footprint node is added to the model (create /
    // undo-delete)
    void addedToModelCb();
    // Callback when the footprint node is removed from model (delete)
    void removedFromModelCb();

    // Get sparse list of hidden cubes.
    HiddenCubes hiddenCubes() const;

    // Set sparse list of hidden cubes.
    void hideCubes(const HiddenCubes& hidden);

    // Get sparse list of transformed cubes.
    TransformedCubes transformedCubes() const;

    // Set cube translation.
    void translate(const std::string& cubeName, double x, double y, double z);

    // Get cube translation.
    GfVec3d translation(const std::string& cubeName) const;

    GfVec3d deltaTrans() const;

    Ufe::Path getUfePath() const;

    Ufe::Path getCubeUfePath(const std::string& cubeName) const;
    static Ufe::Path getCubeUfePath(
        const MObject&     locatorObj,
        const std::string& cubeName
    );

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

    // Set the translation for a given cube.
    void setTranslatePlug(const MPlug& cubePlug, double x, double y, double z);

    SdfPath _pathPrefix;
    Ufe::Path _appPath{};
};

namespace {

using namespace Ufe;

constexpr char ufeRunTimeName[] = "FlowViewportAPILocatorRunTime";
Ufe::Rtid ufeRunTimeId{0};      // 0 is invalid initial UFE run time ID.

std::set<std::string> split(const std::string& str)
{
    std::set<std::string> tokens;
    std::stringstream ss(str);
    std::string token;
    
    while( ss >> token) {
        tokens.insert(token);
    }
    return tokens;
}

// Pick handler for the locator node.

class LocatorPickHandler : public MayaHydra::PickHandler {
public:

    LocatorPickHandler(MObject& locatorObj) : _locatorObj(locatorObj) {}

    bool handlePickHit(
        const Input& pickInput, Output& pickOutput
    ) const override
    {
        auto cubeUfePath = MhFlowViewportAPILocator::getCubeUfePath(
            _locatorObj, pickInput.pickHit.objectId.GetName());

        // Append the picked object to the UFE selection.
        auto si = Ufe::Hierarchy::createItem(cubeUfePath);
        if (!TF_VERIFY(si))  {
            return false;
        }

        pickOutput.ufeSelection->append(si);
        return true;
    }

private:

    MObject _locatorObj;
};

MhFlowViewportAPILocator* getLocator(const Ufe::Path& cubePath)
{
    if (cubePath.size() <= 1) {
        return nullptr;
    }
    auto locatorDagPath = UfeExtensions::ufeToDagPath(cubePath.pop());
    MFnDependencyNode fn(locatorDagPath.node());
    return dynamic_cast<MhFlowViewportAPILocator*>(fn.userNode());
}

    std::string GetStringAttributeValue(const MPlug& plug);

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
        if (plug == flowViewportAPIMayaLocator->mCubeInitialTransform){
            auto dataHandle                                                 = plug.asMDataHandle();
            const MMatrix   mat                                             = dataHandle.asMatrix();
            memcpy(flowViewportAPIMayaLocator->_cubeGridParams._initialTransform.GetArray(), mat[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d
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
        }else
        if (plug == flowViewportAPIMayaLocator->mHiddenCubes){
            flowViewportAPIMayaLocator->_cubeGridParams._hidden = split(GetStringAttributeValue(plug));
        }else
        // Cube transform plugs: the translate plug itself never changes,
        // only its x, y, z children.
        if (plug == flowViewportAPIMayaLocator->mTransformedCubes ||
            plug == flowViewportAPIMayaLocator->mTransformedCubeName ||
            plug == flowViewportAPIMayaLocator->mCubeTranslateX ||
            plug == flowViewportAPIMayaLocator->mCubeTranslateY ||
            plug == flowViewportAPIMayaLocator->mCubeTranslateZ) {

            flowViewportAPIMayaLocator->_cubeGridParams._transformed = 
                flowViewportAPIMayaLocator->transformedCubes();

            // Notify UFE Transform3d observers that a cube transform has
            // changed.  We do so centrally on attribute change so that any
            // modifier of cube translate data (API, scripting, undo / redo,
            // manipulator) will cause a UFE notification to be emitted.
            if (plug == flowViewportAPIMayaLocator->mCubeTranslateX ||
                plug == flowViewportAPIMayaLocator->mCubeTranslateY ||
                plug == flowViewportAPIMayaLocator->mCubeTranslateZ) {

                // Walk up to the translate plug, then up to the transformed
                // cubes plug, then down to the cube name plug.
                MPlug transformedCubesPlug(plug.parent().parent());
                TF_AXIOM(transformedCubesPlug.isElement());
                auto cubeNamePlug = transformedCubesPlug.child(MhFlowViewportAPILocator::mTransformedCubeName);
                std::string cubeName = cubeNamePlug.asString().asChar();
                
                // During translate manipulation the x, y, and z plugs are
                // modified in turn, which causes 3x notification, unclear how
                // to optimize this as of 6-Jun-2024.
                Ufe::Transform3d::notify(flowViewportAPIMayaLocator->getCubeUfePath(cubeName));
            }

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

    std::string GetStringAttributeValue(const MPlug& plug)
    {
        return std::string(plug.asString().asChar());
    }

    //Callback after a File Open
    void afterOpenCallback (void *clientData) 
    {
        if (! clientData){
            return;
        }

        MhFlowViewportAPILocator* flowViewportAPIMayaLocator = reinterpret_cast<MhFlowViewportAPILocator*>(clientData);
        flowViewportAPIMayaLocator->setCubeGridParametersFromAttributes();
        // No need to call flowViewportAPIMayaLocator->addedToModelCb(),
        // as reading the file will add the node to the model.
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

// Minimal UFE scene item implementation, to be included in UFE selection.
class CubeSceneItem : public Ufe::SceneItem
{
public:
    typedef std::shared_ptr<CubeSceneItem> Ptr;

    CubeSceneItem(const Ufe::Path& path) 
        : SceneItem(path), _locator(getLocator(path)) { TF_AXIOM(_locator); }

    std::string nodeType() const override { return "FlowViewportAPILocatorCube"; }

    // Returns the locator parent of the cube.
    MhFlowViewportAPILocator* locator() const { return _locator; }

    // Unimplemented defaults.  These should preferably be in UFE.
    // PPT, 6-Jun-2024.
    Ufe::Value getMetadata(const std::string& key) const override { return {}; }
    UndoableCommandPtr setMetadataCmd(const std::string& key, const Ufe::Value& value) override { return nullptr; }
    UndoableCommandPtr clearMetadataCmd(const std::string& key) override { return nullptr; }
    Ufe::Value getGroupMetadata(const std::string& group, const std::string& key) const override { return {}; }
    UndoableCommandPtr setGroupMetadataCmd(const std::string& group, const std::string& key, const Ufe::Value& value) override { return nullptr; }
    UndoableCommandPtr clearGroupMetadataCmd(const std::string& group, const std::string& key) override { return nullptr; }

private:

    MhFlowViewportAPILocator* const _locator;
};

// Minimal Hierarchy interface handler for locator cubes.  Its only
// responsibility is to create a scene item for a locator cube.
class CubeHierarchyHandler : public Ufe::HierarchyHandler
{
public:

    // No hierarchy interface for locator cubes.
    Hierarchy::Ptr hierarchy(const SceneItem::Ptr& item) const override {
        return nullptr;
    }

    SceneItem::Ptr createItem(const Path& path) const override {
        // Is the argument path an MhFlowViewportAPILocator node?
        return getLocator(path) ? std::make_shared<CubeSceneItem>(path) : nullptr;
    }

    // No children for locator cubes, so no child filter.
    Hierarchy::ChildFilter childFilter() const override { return {}; }
};    

// UFE command for visibility change undo / redo.
class CubeUndoVisibleCommand : public Ufe::UndoableCommand
{
public:
    CubeUndoVisibleCommand(const Path& cubePath, bool newVis, bool oldVis) 
        : _cubePath(cubePath), _newVis(newVis), _oldVis(oldVis)
    {}

private:

    void setVisibility(bool vis) {
        auto item = Hierarchy::createItem(_cubePath);
        if (!TF_VERIFY(item)) { return; }
        auto o3d = Object3d::object3d(item);
        if (!TF_VERIFY(o3d)) { return; }
        o3d->setVisibility(vis);
    }

    void execute() override { redo(); }
    void undo() override {
        setVisibility(_oldVis);
    }
    void redo() override {
        setVisibility(_newVis);
    }

    const Path _cubePath;
    const bool _newVis;
    const bool _oldVis;
};

// Minimal Object3d interface for locator cubes.  It only implements show /
// hide.  If framing is desired, the bounding box method could be implemented.
//
// Only visibility support is implemented as of 28-May-2024.  A sparse list of
// hidden cubes is stored in the Maya locator node.  If our name isn't in the
// hidden list, we're visible.
    
class CubeObject3d : public Ufe::Object3d
{
public:
    
    CubeObject3d(const CubeSceneItem::Ptr& item) : _item(item) {}
    ~CubeObject3d() override = default;

    SceneItem::Ptr sceneItem() const override { return _item; }
    CubeSceneItem::Ptr cubeSceneItem() const { return _item; }
    bool visibility() const override {
        auto hidden = cubeSceneItem()->locator()->hiddenCubes();

        // If we're not on the list, we're visible.
        return hidden.count(sceneItem()->nodeName()) == 0;
    }

    // Set visibility for this cube.  No-op changes do not write to the Maya
    // locator node.
    void setVisibility(bool vis) override {
        auto hidden = cubeSceneItem()->locator()->hiddenCubes();

        auto cubeName = sceneItem()->nodeName();
        // If making visible, try removing from the hidden set, else (making
        // invisible) try adding to the hidden set.
        bool update = (vis ? (hidden.erase(cubeName) > 0) :
                       hidden.insert(cubeName).second);

        if (update) {
            cubeSceneItem()->locator()->hideCubes(hidden);
        }
    }

    Ufe::UndoableCommand::Ptr setVisibleCmd(bool vis) override {
        // In Maya calling hide on an already-hidden object is legal, and
        // logs a no-op undoable command.
        return std::make_shared<CubeUndoVisibleCommand>(
            sceneItem()->path(), vis, visibility());
    }

    // Unimplemented.
    BBox3d boundingBox() const override { return {}; }

private:

    const CubeSceneItem::Ptr _item;
};

class CubeObject3dHandler : public Ufe::Object3dHandler
{
public:
    Object3d::Ptr object3d(const SceneItem::Ptr& item) const override
    {
        return std::make_shared<CubeObject3d>(
            std::dynamic_pointer_cast<CubeSceneItem>(item));
    }
};

class CubeTranslateCommand : public Ufe::TranslateUndoableCommand
{
public:
    CubeTranslateCommand(
        const Path& cubePath, const Vector3d& newT, const Vector3d& oldT
    ) : TranslateUndoableCommand(cubePath), _newT(newT), _oldT(oldT)
    {}

private:

    bool set(double x, double y, double z) override {
        auto item = sceneItem();
        if (!TF_VERIFY(item)) { return false; }
        auto t3d = Transform3d::transform3d(item);
        if (!TF_VERIFY(t3d)) { return false; }
        t3d->translate(x, y, z);
        return true;
    }

    void execute() override { redo(); }
    void undo() override {
        set(_oldT.x(), _oldT.y(), _oldT.z());
    }
    void redo() override {
        set(_newT.x(), _newT.y(), _newT.z());
    }

    const Path     _cubePath;
    const Vector3d _newT;
    const Vector3d _oldT;
};

// Minimal Transform3d interface for locator cubes.  It only implements
// translation.  A sparse list of transformed cubes is stored in the Maya
// locator node.
//
// The cube local transformation is composed of two parts:
// - The cube's position in the grid, as determined by its (x, y, z) indices
//   and the delta translation between cubes.  This acts as a fixed rotate and
//   scale pivot (if rotation and scaling were to be added).
// - The optional per-cube translation.

class CubeTransform3d : public Ufe::Transform3d
{
public:

    CubeTransform3d(const CubeSceneItem::Ptr& item) 
        : _item(item), _gridOffset(gridOffset()) {}

    // Overrides.
    const Path& path() const override {
        static Path emptyPath;
        return _item ? _item->path() : emptyPath;
    }

    SceneItem::Ptr sceneItem() const override { return _item; }
    CubeSceneItem::Ptr cubeSceneItem() const { return _item; }

    Matrix4d matrix() const override {
        GfMatrix4d m(1.0);

        // The local transform matrix is the pivot plus the translation.
        m.SetTranslateOnly(
            UfeExtensions::toUsd(rotatePivot()) + 
            UfeExtensions::toUsd(translation()));

        return UfeExtensions::toUfe(m);
    }

    Matrix4d segmentInclusiveMatrix() const override {
        // Since the cube path segment has only one component (the cube
        // itself), this is simply equal to matrix().
        return matrix();
    }

    Matrix4d segmentExclusiveMatrix() const override {
        // Since the cube path segment has only one component (the cube
        // itself), this is simply the identity matrix.
        return UfeExtensions::toUfe(GfMatrix4d(1.0));
    }

    TranslateUndoableCommand::Ptr translateCmd(
        double x, double y, double z) override {
        return std::make_shared<CubeTranslateCommand>(
            path(), Vector3d(x, y, z), translation());
    }

    void translate(double x, double y, double z) override {
        cubeSceneItem()->locator()->translate(
            sceneItem()->nodeName(), x, y, z);
    }

    Vector3d translation() const override {
        return UfeExtensions::toUfe(
            cubeSceneItem()->locator()->translation(sceneItem()->nodeName()));
    }

    Vector3d rotatePivot() const override {
        return UfeExtensions::toUfe(gridOffset());
    }
    Vector3d scalePivot() const override {
        return rotatePivot();
    }

    // Unimplemented.
    RotateUndoableCommand::Ptr rotateCmd(
        double x, double y, double z) override { return nullptr; }
    Vector3d rotation() const override { return {}; }
    ScaleUndoableCommand::Ptr scaleCmd(
        double x, double y, double z) override { return nullptr; }
    Vector3d scale() const  override { return {1, 1, 1}; }
    TranslateUndoableCommand::Ptr rotatePivotCmd(
        double x, double y, double z) override { return nullptr; }
    TranslateUndoableCommand::Ptr scalePivotCmd(
        double x, double y, double z) override { return nullptr; }
    SetMatrix4dUndoableCommand::Ptr setMatrixCmd(const Matrix4d& m) override
    { return nullptr; }

private:

    Ufe::Vector3i indices() const {
        // Extract (x, y, z) indices from cube name, which is
        // cube_x_y_z.
        const static std::regex re("cube_([0-9]+)_([0-9]+)_([0-9]+)$");
        std::smatch match;
        // Can't match temporary string, see
        // https://stackoverflow.com/questions/27391016
        std::string cn = sceneItem()->nodeName();
        if (!TF_VERIFY(std::regex_match(cn, match, re), 
                       "Illegal cube names without positional indices.")) {
            return {};
        }
        return {std::stoi(match[1]), std::stoi(match[2]), std::stoi(match[3])};
    }

    GfVec3d gridOffset() const {
        // Get the delta translation from the locator node.
        auto dt = cubeSceneItem()->locator()->deltaTrans();
        auto i = indices();
        return GfCompMult(dt, GfVec3d(i.x(), i.y(), i.z()));
    }

    const CubeSceneItem::Ptr _item;
    const GfVec3d            _gridOffset;
};

class CubeTransform3dHandler : public Ufe::Transform3dHandler
{
public:

    Transform3d::Ptr transform3d(const SceneItem::Ptr& item) const override
    {
        return std::make_shared<CubeTransform3d>(
            std::dynamic_pointer_cast<CubeSceneItem>(item));
    }
};

}//end of anonymous namespace

//Initialization of static members
MTypeId MhFlowViewportAPILocator::id( 0x58000993 );
MString	MhFlowViewportAPILocator::nodeClassification("hydraAPIExample/geometry/MhFlowViewportAPILocator");
MObject MhFlowViewportAPILocator::mNumCubeLevelsX;
MObject MhFlowViewportAPILocator::mNumCubeLevelsY;
MObject MhFlowViewportAPILocator::mNumCubeLevelsZ;
MObject MhFlowViewportAPILocator::mCubeHalfSize;
MObject MhFlowViewportAPILocator::mCubeInitialTransform;
MObject MhFlowViewportAPILocator::mCubeColor;
MObject MhFlowViewportAPILocator::mCubeOpacity;
MObject MhFlowViewportAPILocator::mCubesUseInstancing;
MObject MhFlowViewportAPILocator::mCubesDeltaTrans;
MObject MhFlowViewportAPILocator::mHiddenCubes;
MObject MhFlowViewportAPILocator::mCubeTranslateX;
MObject MhFlowViewportAPILocator::mCubeTranslateY;
MObject MhFlowViewportAPILocator::mCubeTranslateZ;
MObject MhFlowViewportAPILocator::mCubeTranslate;
MObject MhFlowViewportAPILocator::mTransformedCubeName;
MObject MhFlowViewportAPILocator::mTransformedCubes;

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
    GetMatrixAttributeValue(mat, mObj, MhFlowViewportAPILocator::mCubeInitialTransform);
    memcpy(_cubeGridParams._initialTransform.GetArray(), mat[0], sizeof(double) * 16);//convert from MMatrix to GfMatrix4d

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

    _cubeGridParams._hidden = split(GetStringAttributeValue(MPlug(mObj, mHiddenCubes)));

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
    MPxLocatorNode::getCacheSetup(evalNode, disablingInfo, cacheSetupInfo, monitoredAttributes);
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

Ufe::Path MhFlowViewportAPILocator::getUfePath() const
{
    MDagPath dagPath;
    TF_AXIOM(MDagPath::getAPathTo(thisMObject(), dagPath) == MS::kSuccess);
    return Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath));
}

/* static */
Ufe::Path MhFlowViewportAPILocator::getCubeUfePath(
    const MObject&     locatorObj,
    const std::string& cubeName
)
{
    Ufe::Path::Segments segments;
    segments.reserve(2);

    // First path segment: Dag path to the locator node.
    MDagPath dagPath;
    TF_AXIOM(MDagPath::getAPathTo(locatorObj, dagPath) == MS::kSuccess);
    segments.emplace_back(UfeExtensions::dagPathToUfePathSegment(dagPath));

    // Second path segment: a single component, the cube identifier.
    segments.emplace_back(Ufe::PathComponent(cubeName), ufeRunTimeId, '/');

    return Ufe::Path(std::move(segments));
}

Ufe::Path
MhFlowViewportAPILocator::getCubeUfePath(const std::string& cubeName) const
{
    return getCubeUfePath(thisMObject(), cubeName);
}

void MhFlowViewportAPILocator::addedToModelCb()
{
    //Add the callback when an attribute of this node changes
    MObject obj = thisMObject();
    _cbAttributeChangedId = MNodeMessage::addAttributeChangedCallback(obj, attributeChangedCallback, ((void*)this));

    _hydraViewportDataProducerSceneIndexExample.setContainerNode(&obj);

    // Construct our scene below a prefix in the Hydra scene.  Would have liked
    // to call
    // GetMayaHydraLibInterface().GetTerminalSceneIndices();
    // to compute a unique, descriptive scene index prefix while accounting for
    // existing prefixes, using
    // MayaHydra::sceneIndexPathPrefix()
    // but during file read this is not possible, as scene indices are built
    // later, and GetTerminalSceneIndices() returns an empty vector.  Use a
    // pointer value to make the prefix unique, even if this is not very
    // readable.
    _pathPrefix = SdfPath(TfStringPrintf("/cube_%p", this));
    _hydraViewportDataProducerSceneIndexExample.addDataProducerSceneIndex(
        _pathPrefix);

    //Store the MObject* of the maya node in various classes
    _hydraViewportFilteringSceneIndexClientExample->setDccNode(&obj);

    //Register this filtering scene index client, so it can append custom filtering scene indices to Hydra viewport scene indices
    Fvp::FilteringSceneIndexInterface& filteringSceneIndexInterface = Fvp::FilteringSceneIndexInterface::get();
    TF_VERIFY(filteringSceneIndexInterface.registerFilteringSceneIndexClient(_hydraViewportFilteringSceneIndexClientExample));

    // Register a pick handler for our prefix with the pick handler registry.
    auto pickHandler = std::make_shared<LocatorPickHandler>(obj);
    TF_AXIOM(MayaHydra::PickHandlerRegistry::Instance().Register(_pathPrefix, pickHandler));

    // Register a path mapper to map application UFE paths to scene index paths,
    // for selection highlighting.
    _appPath = getUfePath();
    auto pathMapper = std::make_shared<Fvp::PrefixPathMapper>(
        ufeRunTimeId, _appPath, _pathPrefix);
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Register(_appPath, pathMapper));
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

    // Unregister our pick handler.
    TF_AXIOM(MayaHydra::PickHandlerRegistry::Instance().Unregister(_pathPrefix));

    // Unregister our path mapper.  Use stored UFE path, as at this point
    // our locator node is no longer in the Maya scene, so we cannot obtain
    // an MDagPath for it.
    TF_AXIOM(Fvp::PathMapperRegistry::Instance().Unregister(_appPath));
}

MhFlowViewportAPILocator::TransformedCubes
MhFlowViewportAPILocator::transformedCubes() const
{
    // On the assumption that the array of transformed cubes is small and few
    // cubes are transformed, read the whole array.
    MPlug transformedCubesPlug(thisMObject(), mTransformedCubes);
    TransformedCubes transformedCubes;
    TF_AXIOM(transformedCubesPlug.isArray());

    for (unsigned int i=0; i < transformedCubesPlug.numElements(); ++i) {
        auto cubePlug = transformedCubesPlug[i];
        auto cubeNamePlug = cubePlug.child(mTransformedCubeName);
        auto cubeTranslatePlug = cubePlug.child(mCubeTranslate);
        auto cubeTxPlug = cubeTranslatePlug.child(mCubeTranslateX);
        auto cubeTyPlug = cubeTranslatePlug.child(mCubeTranslateY);
        auto cubeTzPlug = cubeTranslatePlug.child(mCubeTranslateZ);

        std::string cubeName = cubeNamePlug.asString().asChar();
        GfVec3f cubeTranslate(
            cubeTxPlug.asFloat(), cubeTyPlug.asFloat(), cubeTzPlug.asFloat());
        transformedCubes[cubeName] = cubeTranslate;
    }
    return transformedCubes;
}

void MhFlowViewportAPILocator::setTranslatePlug(
    const MPlug& cubePlug, double x, double y, double z
)
{
    auto cubeTranslatePlug = cubePlug.child(mCubeTranslate);
    cubeTranslatePlug.child(mCubeTranslateX).setValue(x);
    cubeTranslatePlug.child(mCubeTranslateY).setValue(y);
    cubeTranslatePlug.child(mCubeTranslateZ).setValue(z);
}

void MhFlowViewportAPILocator::translate(
    const std::string& cubeName, double x, double y, double z
)
{
    // Check if this cube already has an entry; if so, update it.
    MPlug transformedCubesPlug(thisMObject(), mTransformedCubes);
    TF_AXIOM(transformedCubesPlug.isArray());

    bool found = false;
    MPlug cubePlug;
    for (unsigned int i=0; i < transformedCubesPlug.numElements() && !found;
         ++i) {
        cubePlug = transformedCubesPlug[i];
        auto cubeNamePlug = cubePlug.child(mTransformedCubeName);
        std::string cn = cubeNamePlug.asString().asChar();
        if (cn == cubeName) {
            found = true;
        }
    }

    if (!found) {
        // Add an entry to the array.
        cubePlug = transformedCubesPlug.elementByLogicalIndex(
            transformedCubesPlug.numElements());

        auto cubeNamePlug = cubePlug.child(mTransformedCubeName);
        cubeNamePlug.setValue(cubeName.c_str());
    }

    setTranslatePlug(cubePlug, x, y, z);
}

GfVec3d
MhFlowViewportAPILocator::translation(const std::string& cubeName) const
{
    auto tc = transformedCubes();
    auto found = tc.find(cubeName);
    return (found == tc.end() ? GfVec3d() : found->second);
}

MhFlowViewportAPILocator::HiddenCubes
MhFlowViewportAPILocator::hiddenCubes() const
{
    return split(GetStringAttributeValue(
        MPlug(thisMObject(), MhFlowViewportAPILocator::mHiddenCubes)));
}

void MhFlowViewportAPILocator::hideCubes(const HiddenCubes& hidden)
{
    // Concatenate the set into a space-separated string, and write to the
    // plug.
    constexpr const char* space = " ";
    std::ostringstream newHidden;
    std::copy(hidden.cbegin(), hidden.cend(), 
              std::ostream_iterator<std::string>(newHidden, space));
    MPlug(thisMObject(), mHiddenCubes).setString(MString(newHidden.str().c_str()));
}

GfVec3d MhFlowViewportAPILocator::deltaTrans() const
{
    const auto& dt = MPlug(thisMObject(), mCubesDeltaTrans).asMDataHandle().asDouble3();
    return GfVec3d(dt[0], dt[1], dt[2]);
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
    
    mCubeInitialTransform = mAttr.create("cubeInitalTransform", "cIT", MFnMatrixAttribute::kDouble, &status);
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

    MFnTypedAttribute strAttr;
    mHiddenCubes = strAttr.create("hiddenCubes", "hc", MFnData::kString);
    MAKE_INPUT(strAttr);

    mCubeTranslateX = nAttr.create("translateX", "tx", MFnNumericData::kDouble);
    MAKE_INPUT(nAttr);
    mCubeTranslateY = nAttr.create("translateY", "ty", MFnNumericData::kDouble);
    MAKE_INPUT(nAttr);
    mCubeTranslateZ = nAttr.create("translateZ", "tz", MFnNumericData::kDouble);
    MAKE_INPUT(nAttr);

    MFnCompoundAttribute cAttr;
    mCubeTranslate = cAttr.create("translate", "t");
    cAttr.addChild(mCubeTranslateX);
    cAttr.addChild(mCubeTranslateY);
    cAttr.addChild(mCubeTranslateZ);
    MAKE_INPUT(cAttr);

    mTransformedCubeName = strAttr.create("transformedCubeName", "tcn", MFnData::kString);
    MAKE_INPUT(strAttr);

    mTransformedCubes = cAttr.create("transformedCubes", "tc");
    cAttr.addChild(mTransformedCubeName);
    cAttr.addChild(mCubeTranslate);
    cAttr.setArray(true);
    MAKE_INPUT(cAttr);

    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsX));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsY));
    CHECK_MSTATUS ( addAttribute(mNumCubeLevelsZ));
    CHECK_MSTATUS ( addAttribute(mCubeHalfSize));
    CHECK_MSTATUS ( addAttribute(mCubeInitialTransform));
    CHECK_MSTATUS ( addAttribute(mCubeColor));
    CHECK_MSTATUS ( addAttribute(mCubeOpacity));
    CHECK_MSTATUS ( addAttribute(mCubesUseInstancing));
    CHECK_MSTATUS ( addAttribute(mCubesDeltaTrans));
    CHECK_MSTATUS ( addAttribute(mHiddenCubes));
    CHECK_MSTATUS ( addAttribute(mTransformedCubes));
    
    return status;
}

MStatus initializePlugin( MObject obj )
{
    MStatus   status;
    static const char * pluginVersion = "1.0";
    MFnPlugin plugin( obj, PLUGIN_COMPANY, pluginVersion, "Any");

    status = plugin.registerNode(
                "MhFlowViewportAPILocator",
                MhFlowViewportAPILocator::id,
                &MhFlowViewportAPILocator::creator,
                &MhFlowViewportAPILocator::initialize,
                MPxNode::kLocatorNode,
                &MhFlowViewportAPILocator::nodeClassification);
    if (!status) {
        status.perror("registerNode");
        return status;
    }

    // Register a UFE run-time for the locator node type.  The Hierarchy
    // handler is supported for scene item creation only.
    //
    // Supported UFE interfaces:
    // - Object3d: only visibility supported as of 30-May-2024; bounding box 
    //   unsupported.
    // - Transform3d: only translation supported as of 3-Jun-2024.
    //
    Ufe::RunTimeMgr::Handlers ufeHandlers;
    ufeHandlers.hierarchyHandler = std::make_shared<CubeHierarchyHandler>();
    ufeHandlers.object3dHandler = std::make_shared<CubeObject3dHandler>();
    ufeHandlers.transform3dHandler = std::make_shared<CubeTransform3dHandler>();
    ufeRunTimeId = Ufe::RunTimeMgr::instance().register_(ufeRunTimeName, ufeHandlers);
    // Arbitrarily use '/' as a path string component separator, will never be
    // more than one component.
    Ufe::PathString::registerPathComponentSeparator(ufeRunTimeId, '/');

    return status;
}

MStatus uninitializePlugin( MObject obj)
{
    MStatus   status;
    MFnPlugin plugin( obj );

    Ufe::PathString::unregisterPathComponentSeparator(ufeRunTimeId, '/');

    // Unregister UFE run-time for the locator node type.
    Ufe::RunTimeMgr::instance().unregister(ufeRunTimeId);

    status = plugin.deregisterNode( MhFlowViewportAPILocator::id );
    if (!status) {
        status.perror("deregisterNode");
        return status;
    }
    return status;
}
