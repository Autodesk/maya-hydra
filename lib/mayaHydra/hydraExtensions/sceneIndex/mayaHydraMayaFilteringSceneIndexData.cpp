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
#include "mayaHydraLib/hydraUtils.h"
#include "mayaHydraLib/mayaUtils.h"

//flow viewport headers
#include <flowViewport/API/fvpFilteringSceneIndexClient.h>
#include <flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h>

PXR_NAMESPACE_USING_DIRECTIVE

class MayaFilteringSceneIndexData::UfeSceneChangesHandler : public Ufe::Observer
{
public:
    UfeSceneChangesHandler(MayaFilteringSceneIndexData& filteringData)
        : _filteringData(filteringData)
    {
    }

    void operator()(const Ufe::Notification& notification) override;
    void handleSceneChanged(const Ufe::SceneChanged& sceneChanged);

private:
    MayaFilteringSceneIndexData& _filteringData;
};

MayaFilteringSceneIndexData::MayaFilteringSceneIndexData(const std::shared_ptr<::FVP_NS_DEF::FilteringSceneIndexClient>& client)
: PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBase(client)
{
    void* dccNode = client->getDccNode();
    if (dccNode) {
        SetupUfeObservation(dccNode);
    }
}

MayaFilteringSceneIndexData::~MayaFilteringSceneIndexData()
{
    if (_ufeSceneChangesHandler) {
        Ufe::Scene::instance().removeObserver(_ufeSceneChangesHandler);
        _ufeSceneChangesHandler.reset();
    }
}

void MayaFilteringSceneIndexData::SetupUfeObservation(void* dccNode)
{
    // If the filter is based on a scene item, monitor changes to it to reflect them on
    // the filtering scene index.
    if (dccNode) {
        MObject* mObject = reinterpret_cast<MObject*>(dccNode);
        MDagPath dagPath;
        MDagPath::getAPathTo(*mObject, dagPath);
        dagPath.extendToShape();

        _path = Ufe::Path(UfeExtensions::dagPathToUfePathSegment(dagPath));

        _ufeSceneChangesHandler = std::make_shared<UfeSceneChangesHandler>(*this);
        Ufe::Scene::instance().addObserver(_ufeSceneChangesHandler);

        // Note : while we currently use a query-based approach to update the visibility,
        // we could also move to a UFE notifications-based approach if necessary. In this case,
        // we would setup the subject-observer relationships here. The observer would observe
        // the Ufe::Object3d subject, receive Ufe::VisibilityChanged notifications and call
        // UpdateVisibility() if the received notification is relevant (i.e. if the filtering 
        // scene index's path starts with the notification's path, the same way as in
        // MayaFilteringSceneIndexData::UfeSceneChangesHandler::operator()).
    }
}

bool MayaFilteringSceneIndexData::UpdateVisibility()
{
    if (!_path.has_value()) {
        return false;
    }

    bool      isVisible = true;
    Ufe::Path currPath = _path.value();
    while (isVisible && !currPath.empty()) {
        auto sceneItem = Ufe::Hierarchy::createItem(currPath);
        auto object3d = Ufe::Object3d::object3d(sceneItem);
        isVisible = isVisible && object3d != nullptr && object3d->visibility();
        currPath = currPath.pop();
    }
    if (_isVisible != isVisible) {
        _isVisible = isVisible;
        return true;
    }
    return false;
}

void MayaFilteringSceneIndexData::UfeSceneChangesHandler::operator()(
    const Ufe::Notification& notification)
{
    // We're processing UFE notifications, which implies that a path must be in use.
    TF_AXIOM(_filteringData._path.has_value());

    const Ufe::SceneChanged& sceneChangedNotif = notification.staticCast<Ufe::SceneChanged>();
    if (_filteringData._path.value().startsWith(sceneChangedNotif.changedPath())) {
        handleSceneChanged(sceneChangedNotif);
    }
}

void MayaFilteringSceneIndexData::UfeSceneChangesHandler::handleSceneChanged(
    const Ufe::SceneChanged& sceneChanged)
{
    auto handleSingleOperation
        = [&](const Ufe::SceneCompositeNotification::Op& sceneOperation) -> void {
        // We're processing UFE notifications, which implies that a path must be in use.
        TF_AXIOM(_filteringData._path.has_value());

        if (!_filteringData._path.value().startsWith(sceneOperation.path)) {
            // This notification does not relate to our parent hierarchy, so we have nothing to do.
            return;
        }

        if (sceneOperation.opType == Ufe::SceneChanged::ObjectPathChange) {
            switch (sceneOperation.subOpType) {
            case Ufe::ObjectPathChange::ObjectRename: {
                _filteringData._path = _filteringData._path.value().replaceComponent(
                    sceneOperation.item->path().size() - 1, sceneOperation.item->path().back());
                break;
            }
            case Ufe::ObjectPathChange::ObjectReparent: {
                _filteringData._path = _filteringData._path.value().reparent(
                    sceneOperation.path, sceneOperation.item->path());
                break;
            }
            }
        }
    };
    if (sceneChanged.opType() == Ufe::SceneChanged::SceneCompositeNotification) {
        const auto& compositeNotification
            = sceneChanged.staticCast<Ufe::SceneCompositeNotification>();
        for (const auto& operation : compositeNotification) {
            handleSingleOperation(operation);
        }
    } else {
        handleSingleOperation(sceneChanged);
    }
}
