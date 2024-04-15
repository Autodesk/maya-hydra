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

//Maya headers
#include <maya/MDGMessage.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MFnAttribute.h>
#include <maya/MStringArray.h>
#include <maya/MPlug.h>
#include <maya/MDagPath.h>

PXR_NAMESPACE_USING_DIRECTIVE

class MayaFilteringSceneIndexData::UfeNotificationsHandler : public Ufe::Observer
{
public:
    UfeNotificationsHandler(MayaFilteringSceneIndexData& filteringData)
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
    // If a Maya node is present in client->getDccNode(), add callbacks to handle node added/deleted/reparented/renamed and hide/unhide
    void* dccNode = client->getDccNode();
    if (dccNode) {
        SetupUfeObservation(dccNode);
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

        _notificationsHandler = std::make_shared<UfeNotificationsHandler>(*this);

        Ufe::Scene::instance().addObserver(_notificationsHandler); // For hierarchy changes
        Ufe::Object3d::addObserver(_notificationsHandler);         // For visibility changes

        UpdateVisibility();
    }
}

void MayaFilteringSceneIndexData::UpdateVisibility()
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

void MayaFilteringSceneIndexData::UfeNotificationsHandler::operator()(
    const Ufe::Notification& notification)
{
    // We're processing UFE notifications, which implies that a path must be in use.
    TF_AXIOM(_filteringData._path.has_value());

    const Ufe::VisibilityChanged* visibilityChangedNotif
        = dynamic_cast<const Ufe::VisibilityChanged*>(&notification);
    if (visibilityChangedNotif != nullptr
        && _filteringData._path.value().startsWith(visibilityChangedNotif->path())) {
        _filteringData.UpdateVisibility();
        return;
    }

    const Ufe::SceneChanged* sceneChangedNotif
        = dynamic_cast<const Ufe::SceneChanged*>(&notification);
    if (sceneChangedNotif != nullptr
        && _filteringData._path.value().startsWith(sceneChangedNotif->changedPath())) {
        handleSceneChanged(*sceneChangedNotif);
        return;
    }

    // 2024-04-11 : The two main types of notifications being handled here (VisibilityChanged and SceneChanged)
    // are all sent from two different subjects. We share the same observer for all subjects for simplicity, 
    // but if we ever want to avoid cascading dynamic casts, we could instead use a dedicated observer for each 
    // subject, and use static casts instead.
}

void MayaFilteringSceneIndexData::UfeNotificationsHandler::handleSceneChanged(
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

        switch (sceneOperation.opType) {
        case Ufe::SceneChanged::ObjectAdd: _filteringData.UpdateVisibility(); break;
        case Ufe::SceneChanged::ObjectDelete: _filteringData.SetVisibility(false); break;
        case Ufe::SceneChanged::ObjectPathChange:
            switch (sceneOperation.subOpType) {
            case Ufe::ObjectPathChange::None: break;
            case Ufe::ObjectPathChange::ObjectRename:
                _filteringData._path = _filteringData._path.value().replaceComponent(
                    sceneOperation.item->path().size() - 1, sceneOperation.item->path().back());
                break;
            case Ufe::ObjectPathChange::ObjectReparent:
                _filteringData._path = _filteringData._path.value().reparent(
                    sceneOperation.path, sceneOperation.item->path());
                _filteringData.UpdateVisibility();
                break;
            case Ufe::ObjectPathChange::ObjectPathAdd:
                TF_WARN("Instancing is not supported for this item.");
                break;
            case Ufe::ObjectPathChange::ObjectPathRemove:
                TF_WARN("Instancing is not supported for this item.");
                break;
            }
            break;
        case Ufe::SceneChanged::SubtreeInvalidate: _filteringData.SetVisibility(false); break;
        case Ufe::SceneChanged::SceneCompositeNotification:
            TF_CODING_ERROR("SceneCompositeNotification cannot be turned into an Op.");
            break;
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
