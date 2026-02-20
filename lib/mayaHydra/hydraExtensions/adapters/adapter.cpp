//
// Copyright 2019 Luma Pictures
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
#include "adapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>

#include <maya/MNodeMessage.h>
#include <maya/MFnAttribute.h>

PXR_NAMESPACE_OPEN_SCOPE

TF_REGISTRY_FUNCTION(TfType) { TfType::Define<MayaHydraAdapter>(); }

namespace {

using LockType = std::recursive_mutex;
LockType dg_access_mutex;

void _preRemoval(MObject& node, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraAdapter*>(clientData);
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Pre-removal callback triggered for prim (%s)\n", adapter->GetID().GetText());
    adapter->GetMayaHydraSceneIndex()->RemoveAdapter(adapter->GetID());
}

void _nameChanged(MObject& node, const MString& str, void* clientData)
{
    TF_UNUSED(node);
    TF_UNUSED(str);
    auto* adapter = reinterpret_cast<MayaHydraAdapter*>(clientData);
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Name-changed callback triggered for prim (%s)\n", adapter->GetID().GetText());
    adapter->RemoveCallbacks();
    adapter->GetMayaHydraSceneIndex()->RecreateAdapterOnIdle(adapter->GetID(), adapter->GetNode());
}

} // namespace

// MayaHydraAdapter is the base class for all adapters. An adapter is used to translate from Maya
// data to hydra data.
MayaHydraAdapter::MayaHydraAdapter(
    const MObject&        node,
    const SdfPath&        id,
    MayaHydraSceneIndex*  mayaHydraSceneIndex)
    : _id(id)
    , _mayaHydraSceneIndex(mayaHydraSceneIndex)
    , _node(node)
{
}

MayaHydraAdapter::~MayaHydraAdapter() { RemoveCallbacks(); }

void MayaHydraAdapter::AddCallback(MCallbackId callbackId) { _callbacks.push_back(callbackId); }

void MayaHydraAdapter::RemoveCallbacks()
{
    if (_callbacks.empty()) {
        return;
    }

    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Removing all adapter callbacks for prim (%s).\n", GetID().GetText());
    for (auto c : _callbacks) {
        MMessage::removeCallback(c);
    }
    std::vector<MCallbackId>().swap(_callbacks);
}

VtValue MayaHydraAdapter::Get(const TfToken& key)
{
    // Get extension attributes
    auto it = _extAttrNameToValueMap.find(key.GetText());
    if (it != _extAttrNameToValueMap.end()) {
        return it->second;
    }
    return {};
};

bool MayaHydraAdapter::HasType(const TfToken& typeId) const
{
    TF_UNUSED(typeId);
    return false;
}

void MayaHydraAdapter::CreateCallbacks()
{
    if (_node != MObject::kNullObj) {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
            .Msg("Creating generic adapter callbacks for prim (%s).\n", GetID().GetText());

        MStatus status;
        auto    id = MNodeMessage::addNodePreRemovalCallback(_node, _preRemoval, this, &status);
        if (status) {
            AddCallback(id);
        }
        id = MNodeMessage::addNameChangedCallback(_node, _nameChanged, this, &status);
        if (status) {
            AddCallback(id);
        }
    }
}

MStatus MayaHydraAdapter::Initialize()
{
    auto status = MayaAttrs::initialize();
    if (status) {
        MayaHydraMaterialNetworkConverter::initialize();
    }
    return status;
}

HdPrimvarDescriptorVector MayaHydraAdapter::GetPrimvarDescriptors(HdInterpolation interpolation)
{
    // All extension attributes as custom primvars
    if (interpolation == HdInterpolationConstant) {
        if (_extAttrMapNeedUpdate) {
            // Apply a global lock to avoid race condition while doing parallel DG node evaluation.
            std::lock_guard<LockType> lock(dg_access_mutex);
            MAYAHYDRA_NS::GetExtensionAttributesFromNode(GetNode(), _extAttrNameToValueMap);
            _extAttrMapNeedUpdate = false;
        }
        // Use constant interpolation and none role for all primvars
        HdPrimvarDescriptorVector descriptors;
        for (auto it = _extAttrNameToValueMap.begin(); it != _extAttrNameToValueMap.end(); it++) {
            descriptors.push_back({ TfToken(it->first), interpolation, HdPrimvarRoleTokens->none });
        }
        return descriptors;
    }
    return HdPrimvarDescriptorVector();
}

void MayaHydraAdapter::HandleExtensionAndDynamicAttributesDirty(const MPlug& plug)
{
    MStatus status;
    MObject attrObj = plug.attribute(&status);
    if (status) {
        MFnAttribute fnAttr(attrObj);
        if (fnAttr.isExtension() || fnAttr.isDynamic()) {
            _extAttrMapNeedUpdate = true;
            // Notify the change tracker that the primvars have changed.
            // Note there's no fine grained dirty notification mechanism on primvars yet,
            // like dirty flags for adding/removing/changing a specific primvar, only
            // DirtyPrimvar for all changes. Currently, no performance bottleneck was spotted
            // around this. This could be improved if a more fine grained mechanism is provided.
            MarkDirty(HdChangeTracker::DirtyPrimvar);
        }
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
