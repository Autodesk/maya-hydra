//
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

#ifndef MAYAHYDRALIB_SCENE_INDEX_REGISTRATION_H
#define MAYAHYDRALIB_SCENE_INDEX_REGISTRATION_H

#include <mayaHydraLib/api.h>

#include <flowViewport/flowViewport.h>

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MCallbackIdArray.h>
#include <maya/MMessage.h>
#include <maya/MNodeMessage.h>
#include <maya/MObject.h>
#include <maya/MObjectHandle.h>
#include <maya/MObjectArray.h>

#include <ufe/rtid.h>
#include <ufe/ufe.h>

#include <atomic>
#include <unordered_map>

UFE_NS_DEF {
class Path;
}

namespace FVP_NS_DEF {
class RenderIndexProxy;
}

PXR_NAMESPACE_OPEN_SCOPE

using MayaHydraSelectionObserverPtr = std::shared_ptr<class MayaHydraSelectionObserver>;
using MayaHydraSceneIndexRegistrationPtr = std::shared_ptr<struct MayaHydraSceneIndexRegistration>;
typedef Ufe::Path (*MayaHydraInterpretRprimPath)(const HdSceneIndexBaseRefPtr&, const SdfPath&);

struct MayaHydraSceneIndexRegistration
{
    HdSceneIndexBaseRefPtr      pluginSceneIndex;
    HdSceneIndexBaseRefPtr      rootSceneIndex;
    SdfPath                     sceneIndexPathPrefix;
    MObjectHandle               dagNode;
    MayaHydraInterpretRprimPath interpretRprimPathFn = nullptr;

    virtual void Update() = 0;
};

/**
 * \brief MayaHydraSceneIndexRegistration is used to register a scene index for a given dag node
 * type.
 *
 * To add a custom scene index, a customer plugin must :
 *  1. Define a Maya dag node via the MPxNode interface, and register it MFnPlugin::registerNode.
 * This is typically node inside a Maya pluging initialize function.
 *  2. Define a HdSceneIndexPlugin which contains an _AppendSceneIndex method. The _AppendSceneIndex
 * method will be called for every Maya node added into the scene. A customer is responsible for
 * type checking the node for the one defined and also instantiate the corresponding scene index
 * inside _AppendSceneIndex. The scene index returned by _AppendSceneIndex is then added to the
 * render index by Maya.
 */
class MayaHydraSceneIndexRegistry
{
public:

    using Registrations = std::unordered_map<SdfPath, MayaHydraSceneIndexRegistrationPtr, SdfPath::Hash>;

    static constexpr Ufe::Rtid kInvalidUfeRtid = 0;
    MAYAHYDRALIB_API
    MayaHydraSceneIndexRegistry(const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy);

    MAYAHYDRALIB_API
    ~MayaHydraSceneIndexRegistry();

    MAYAHYDRALIB_API
    MayaHydraSceneIndexRegistrationPtr
    GetSceneIndexRegistrationForRprim(const SdfPath& rprimPath) const;

    MAYAHYDRALIB_API
    const Registrations& GetRegistrations() const;

private:
    void
    _AddSceneIndexForNode(MObject& dagNode); // dagNode non-const because of callback registration
    bool        _RemoveSceneIndexForNode(const MObject& dagNode);
    static void _SceneIndexNodeAddedCallback(MObject& obj, void* clientData);
    static void _SceneIndexNodeRemovedCallback(MObject& obj, void* clientData);
    static void _AfterOpenCallback (void *clientData);

    // Append a node to the list of nodes that need to be processed after the scene is opened.
    void _AppendNodeToProcessAfterOpenScene(const MObject& node) {_nodesToProcessAfterOpenScene.append(node);}
    //We need to check if some nodes that need to be processed were added to our array during a file load
    void _ProcessNodesAfterOpen();
    void _RemoveAllSceneIndexNodes();

    const std::shared_ptr<Fvp::RenderIndexProxy> _renderIndexProxy;

    MCallbackIdArray _DGCallbackIds;
    MCallbackId _AfterOpenCBId {0};

    // Maintain a list of nodes that need to be processed after the scene is opened. We cannot process them during file load.
    MObjectArray    _nodesToProcessAfterOpenScene;

    Registrations _registrations;
    // Maintain alternative way to retrieve registration based on MObjectHandle. This is faster to
    // retrieve the registration upon callback whose event argument is the node itself.
    struct _HashObjectHandle
    {
        unsigned long operator()(const MObjectHandle& handle) const { return handle.hashCode(); }
    };
    std::unordered_map<MObjectHandle, MayaHydraSceneIndexRegistrationPtr, _HashObjectHandle>
        _registrationsByObjectHandle;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_SCENE_INDEX_REGISTRATION_H
