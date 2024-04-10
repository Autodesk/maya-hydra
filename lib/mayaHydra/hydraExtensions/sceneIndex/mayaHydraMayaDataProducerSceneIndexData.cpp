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
#include <maya/MDagMessage.h>
#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MStringArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MPlug.h>

PXR_NAMESPACE_USING_DIRECTIVE

class MayaDataProducerSceneIndexData::UfeNotificationsHandler : public Ufe::Observer
{
public:
    UfeNotificationsHandler(MayaDataProducerSceneIndexData& dataProducer)
        : _dataProducer(dataProducer)
    {
    }

    void operator()(const Ufe::Notification& notification) override;
    void handleSceneChanged(const Ufe::SceneChanged& sceneChanged);

private:
    MayaDataProducerSceneIndexData& _dataProducer;
};

MayaDataProducerSceneIndexData::MayaDataProducerSceneIndexData(const FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParameters& params) 
    : FVP_NS_DEF::DataProducerSceneIndexDataBase(params)
{
    SetupUfeObservation();
    _CreateSceneIndexChainForDataProducerSceneIndex();
}

MayaDataProducerSceneIndexData::MayaDataProducerSceneIndexData(FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params) 
    : FVP_NS_DEF::DataProducerSceneIndexDataBase(params)
{
    SetupUfeObservation();
    _CreateSceneIndexChainForUsdStageSceneIndex(params);
}

MayaDataProducerSceneIndexData::~MayaDataProducerSceneIndexData()
{
}

std::string MayaDataProducerSceneIndexData::GetDCCNodeName() const
{
    if (!_path.has_value()) {
        return {};
    }
    std::string nodeName = _path.value().back().string();
    MAYAHYDRA_NS_DEF::SanitizeNameForSdfPath(nodeName);
    return nodeName;
}

void MayaDataProducerSceneIndexData::SetupUfeObservation() 
{ 
    // If the data producer is based on a Maya node, monitor changes to the node to reflect them on the data producer scene index.
    if (_dccNode) {
        MObject* mObject = reinterpret_cast<MObject*>(_dccNode);
        MDagPath dagPath;
        MDagPath::getAPathTo(*mObject, dagPath);
        dagPath.extendToShape();

        _path = Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath));

        _pathSubject = std::make_shared<Ufe::Transform3dPathSubject>(_path.value());
        _notificationsHandler = std::make_shared<UfeNotificationsHandler>(*this);

        Ufe::Scene::instance().addObserver(_notificationsHandler); // For hierarchy changes (reparent/add/delete)
        Ufe::Object3d::addObserver(_notificationsHandler); // For visibility changes
        _pathSubject->addObserver(_notificationsHandler); // For transform changes

        UpdateTransform();
        UpdateVisibility();
    }
}

void MayaDataProducerSceneIndexData::UpdateTransform() 
{ 
    if (!_path.has_value()) {
        return;
    }
    auto transform = Ufe::Transform3dRead::transform3dRead(Ufe::Hierarchy::createItem(_path.value()));
    if (!transform) {
        return;
    }
    GfMatrix4d transformMatrix;
    std::memcpy(transformMatrix.GetArray(), transform->inclusiveMatrix().matrix.data(), sizeof(double) * 4 * 4);
    SetTransform(transformMatrix);
}

void MayaDataProducerSceneIndexData::UpdateVisibility()
{
    if (!_path.has_value()) {
        return;
    }

    bool      isVisible = true;
    Ufe::Path currPath = _path.value();
    while (isVisible && !currPath.empty()) {
        auto sceneItem = Ufe::Hierarchy::createItem(currPath);
        auto object3d = Ufe::Object3d::object3d(sceneItem);
        isVisible = isVisible && object3d != nullptr && object3d->visibility();
        currPath = currPath.pop();
    }
    SetVisibility(isVisible);
}

void MayaDataProducerSceneIndexData::UfeNotificationsHandler::operator()(const Ufe::Notification& notification)
{
    // We're processing UFE notifications, which implies that a path must be in use.
    TF_AXIOM(_dataProducer._path.has_value());

    const Ufe::Transform3dChanged* transformChangedNotif = dynamic_cast<const Ufe::Transform3dChanged*>(&notification);
    if (transformChangedNotif != nullptr && _dataProducer._path.value().startsWith(transformChangedNotif->item()->path())) {
        _dataProducer.UpdateTransform();
        return;
    }

    const Ufe::VisibilityChanged* visibilityChangedNotif = dynamic_cast<const Ufe::VisibilityChanged*>(&notification);
    if (visibilityChangedNotif != nullptr && _dataProducer._path.value().startsWith(visibilityChangedNotif->path())) {
        _dataProducer.UpdateVisibility();
        return;
    }

    const Ufe::SceneChanged* sceneChangedNotif = dynamic_cast<const Ufe::SceneChanged*>(&notification);
    if (sceneChangedNotif != nullptr && _dataProducer._path.value().startsWith(sceneChangedNotif->changedPath())) {
        handleSceneChanged(*sceneChangedNotif);
        return;
    }

    // 2024-04-11 : The three main types of notifications being handled here (Transform3dChanged, VisibilityChanged and SceneChanged)
    // are all sent from three different subjects. We share the same observer for all subjects for simplicity, but if we ever want to 
    // avoid cascading dynamic casts, we could instead use a dedicated observer for each subject, and use static casts instead.
}

void MayaDataProducerSceneIndexData::UfeNotificationsHandler::handleSceneChanged(const Ufe::SceneChanged& sceneChanged)
{
    auto handleSingleOperation = [&](const Ufe::SceneCompositeNotification::Op& sceneOperation) -> void {
        // We're processing UFE notifications, which implies that a path must be in use.
        TF_AXIOM(_dataProducer._path.has_value());
        
        if (!_dataProducer._path.value().startsWith(sceneOperation.path)) {
            // This notification does not relate to our parent hierarchy, so we have nothing to do.
            return;
        }

        switch (sceneOperation.opType) {
            case Ufe::SceneChanged::ObjectAdd:
                _dataProducer.UpdateTransform();
                _dataProducer.UpdateVisibility();
                break;
            case Ufe::SceneChanged::ObjectDelete:
                _dataProducer.SetVisibility(false);
                break;
            case Ufe::SceneChanged::ObjectPathChange:
                switch (sceneOperation.subOpType) {
                    case Ufe::ObjectPathChange::None:
                        break;
                    case Ufe::ObjectPathChange::ObjectRename:
                        _dataProducer._path = _dataProducer._path.value().replaceComponent(
                            sceneOperation.item->path().size() - 1,
                            sceneOperation.item->path().back());
                        break;
                    case Ufe::ObjectPathChange::ObjectReparent:
                        _dataProducer._path = _dataProducer._path.value().reparent(sceneOperation.path, sceneOperation.item->path());
                        _dataProducer.UpdateTransform();
                        _dataProducer.UpdateVisibility();
                        break;
                    case Ufe::ObjectPathChange::ObjectPathAdd:
                        TF_WARN("Instancing is not supported for this item.");
                        break;
                    case Ufe::ObjectPathChange::ObjectPathRemove:
                        TF_WARN("Instancing is not supported for this item.");
                        break;
                }
                break;
            case Ufe::SceneChanged::SubtreeInvalidate:
                _dataProducer.SetVisibility(false);
                break;
            case Ufe::SceneChanged::SceneCompositeNotification:
                TF_CODING_ERROR("SceneCompositeNotification cannot be turned into an Op.");
                break;
        }
    };
    if (sceneChanged.opType() == Ufe::SceneChanged::SceneCompositeNotification) {
        const auto& compositeNotification = sceneChanged.staticCast<Ufe::SceneCompositeNotification>();
        for (const auto& operation : compositeNotification) {
            handleSingleOperation(operation);
        }
    } else {
        handleSingleOperation(sceneChanged);
    }
}
