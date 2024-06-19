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

PXR_NAMESPACE_USING_DIRECTIVE

class MayaDataProducerSceneIndexData::UfeSceneChangesHandler : public Ufe::Observer
{
public:
    UfeSceneChangesHandler(MayaDataProducerSceneIndexData& dataProducer)
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
    if (_dccNode) {
        SetupUfeObservation(_dccNode);
    }
    _CreateSceneIndexChainForDataProducerSceneIndex();
}

MayaDataProducerSceneIndexData::MayaDataProducerSceneIndexData(FVP_NS_DEF::DataProducerSceneIndexDataBase::CreationParametersForUsdStage& params) 
    : FVP_NS_DEF::DataProducerSceneIndexDataBase(params)
{
    if (_dccNode) {
        SetupUfeObservation(_dccNode);
    }
    _CreateSceneIndexChainForUsdStageSceneIndex(params);
}

void MayaDataProducerSceneIndexData::SetupUfeObservation(void* dccNode)
{
    // If the data producer is based on a scene item, monitor changes to it to reflect them on the
    // data producer scene index.
    if (dccNode) {
        MObject* mObject = reinterpret_cast<MObject*>(dccNode);
        MDagPath dagPath;
        MDagPath::getAPathTo(*mObject, dagPath);
        dagPath.extendToShape();

        _path = Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath));

        _ufeSceneChangesHandler = std::make_shared<UfeSceneChangesHandler>(*this);
        Ufe::Scene::instance().addObserver(_ufeSceneChangesHandler);

        // Note : while we currently use a query-based approach to update the transform and visibility,
        // we could also move to a UFE notifications-based approach if necessary. In this case, we would
        // setup the subject-observer relationships here.
    }
}

bool MayaDataProducerSceneIndexData::UpdateVisibility()
{
    if (!_path.has_value()) {
        return false;
    }
    // Having a UFE path means we have an associated DCC node,
    // so we should also have a UsdImagingRootOverridesSceneIndex
    TF_AXIOM(_rootOverridesSceneIndex);

    bool      isVisible = true;
    Ufe::Path currPath = _path.value();
    while (isVisible && !currPath.empty()) {
        auto sceneItem = Ufe::Hierarchy::createItem(currPath);
        auto object3d = Ufe::Object3d::object3d(sceneItem);
        isVisible = isVisible && object3d != nullptr && object3d->visibility();
        currPath = currPath.pop();
    }
    if (_rootOverridesSceneIndex->GetRootVisibility() != isVisible) {
        _rootOverridesSceneIndex->SetRootVisibility(isVisible);
        return true;
    }
    return false;
}

bool MayaDataProducerSceneIndexData::UpdateTransform()
{
    if (!_path.has_value()) {
        return false;
    }
    // Having a UFE path means we have an associated DCC node,
    // so we should also have a UsdImagingRootOverridesSceneIndex
    TF_AXIOM(_rootOverridesSceneIndex);

    auto transform = Ufe::Transform3dRead::transform3dRead(Ufe::Hierarchy::createItem(_path.value()));
    if (!transform) {
        return false;
    }
    GfMatrix4d transformMatrix;
    std::memcpy(
        transformMatrix.GetArray(),
        transform->inclusiveMatrix().matrix.data(),
        sizeof(decltype(transformMatrix)::ScalarType) * transformMatrix.numRows * transformMatrix.numColumns);
    if (!GfIsClose(_rootOverridesSceneIndex->GetRootTransform(), transformMatrix, 1e-9)) {
        _rootOverridesSceneIndex->SetRootTransform(transformMatrix);
        return true;
    }
    return false;
}

void MayaDataProducerSceneIndexData::UfeSceneChangesHandler::operator()(const Ufe::Notification& notification)
{
    // We're processing UFE notifications, which implies that a path must be in use.
    TF_AXIOM(_dataProducer._path.has_value());

    const Ufe::SceneChanged& sceneChangedNotif = notification.staticCast<Ufe::SceneChanged>();
    if (_dataProducer._path.value().startsWith(sceneChangedNotif.changedPath())) {
        handleSceneChanged(sceneChangedNotif);
    }
}

void MayaDataProducerSceneIndexData::UfeSceneChangesHandler::handleSceneChanged(const Ufe::SceneChanged& sceneChanged)
{
    auto handleSingleOperation = [&](const Ufe::SceneCompositeNotification::Op& sceneOperation) -> void {
        // We're processing UFE notifications, which implies that a path must be in use.
        TF_AXIOM(_dataProducer._path.has_value());
        // Having a UFE path means we have an associated DCC node,
        // so we should also have a UsdImagingRootOverridesSceneIndex
        TF_AXIOM(_dataProducer._rootOverridesSceneIndex);

        if (!_dataProducer._path.value().startsWith(sceneOperation.path)) {
            // This notification does not relate to our parent hierarchy, so we have nothing to do.
            return;
        }

        switch (sceneOperation.opType) {
        case Ufe::SceneChanged::ObjectAdd:
            _dataProducer.UpdateTransform();
            _dataProducer.UpdateVisibility();
            break;
        case Ufe::SceneChanged::ObjectDelete: _dataProducer._rootOverridesSceneIndex->SetRootVisibility(false); break;
        case Ufe::SceneChanged::ObjectPathChange:
            switch (sceneOperation.subOpType) {
            case Ufe::ObjectPathChange::None: break;
            case Ufe::ObjectPathChange::ObjectRename:
                _dataProducer._path = _dataProducer._path.value().replaceComponent(
                    sceneOperation.item->path().size() - 1, sceneOperation.item->path().back());
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
        case Ufe::SceneChanged::SubtreeInvalidate: _dataProducer._rootOverridesSceneIndex->SetRootVisibility(false); break;
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
