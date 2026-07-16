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
#include <mayaHydraLib/adapters/mhDirtyNotifier.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/renderIndex.h>

#include <maya/MNodeMessage.h>
#include <maya/MFnAttribute.h>
#include <maya/MFileIO.h>

PXR_NAMESPACE_OPEN_SCOPE

// Check if a plug maps to a parameter attribute name.
bool MayaHydraAdapter::IsParamAttribute(const MPlug& plug, const std::unordered_set<std::string>& paramAttrs)
{
    MStatus      status;
    MFnAttribute attrFn(plug.attribute(&status));
    if (!status) {
        return false;
    }
    return paramAttrs.count(attrFn.name().asChar()) != 0;
}

TF_REGISTRY_FUNCTION(TfType) { TfType::Define<MayaHydraAdapter>(); }

namespace {

using LockType = std::recursive_mutex;
LockType dg_access_mutex;

// When an extension/dynamic attribute set changes on an rprim, also dirty extComputationPrimvars
// because the attribute may back a computation input. This is NOT emitted on topology changes
// (see doc/render_delegate_topology_vs_deformation.md).
void _maybeDirtyExtComputationPrimvars(MayaHydraAdapter& adapter, Fvp::DirtyNotifier& notifier)
{
    if (adapter.IsRprimTypeSupportedForPrim()) {
        notifier.dirtyExtComputationPrimvars();
    }
}

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

/// Skip Hydra dirty notifications during file read or when the render index is unavailable.
bool MayaHydraAdapter::ShouldSkipHydraUpdates(MayaHydraSceneIndex* sceneIndex)
{
    // During file read, plugins (e.g. mtoa) may add extension attributes that fire
    // adapter callbacks before Hydra resources are in a consistent state.
    if (MFileIO::isOpeningFile()) {
        return true;
    }
    return sceneIndex == nullptr || !sceneIndex->HasRenderDelegate();
}

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

bool MayaHydraAdapter::IsRprimTypeSupportedForPrim() const
{
    // If the prim is not in the scene index the cache is stale regardless of which adapter
    // subclass set it. Clearing here (rather than in every RemovePrim() override) is the
    // single-point fix: any call to IsRprimTypeSupportedForPrim() between RemovePrim() and
    // the next Populate()
    // returns false and resets the flag so the next Populate() re-resolves a fresh value.
    if (!_isPopulated) {
        _isRprimResolved = false;
        return false;
    }
    if (!_isRprimResolved) {
        if (!TF_VERIFY(
                _mayaHydraSceneIndex->HasRenderDelegate(),
                "IsRprimTypeSupportedForPrim() called without a render delegate; callers must "
                "guard with ShouldSkipHydraUpdates() first.")) {
            return false;
        }
        const HdSceneIndexPrim prim = _mayaHydraSceneIndex->GetPrim(_id);
        _isRprimValue    = _mayaHydraSceneIndex->IsRprimTypeSupported(prim.primType);
        _isRprimResolved = true;
    }
    return _isRprimValue;
}

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

// Retrieve the primvar value from the cached extension/dynamic attribute map.
VtValue MayaHydraAdapter::Get(const TfToken& key)
{
    // Get extension/dynamic attributes stored as primvars.
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

// Register generic adapter callbacks (pre-remove and rename).
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

// Initialize shared adapter state (Maya attribute tables, converters).
MStatus MayaHydraAdapter::Initialize()
{
    auto status = MayaAttrs::initialize();
    if (status) {
        MayaHydraMaterialNetworkConverter::initialize();
    }
    return status;
}

// Build primvar descriptors from the cached extension/dynamic attribute map.
HdPrimvarDescriptorVector MayaHydraAdapter::GetPrimvarDescriptors(HdInterpolation interpolation)
{
    // All extension/dynamic attributes as custom primvars.
    if (interpolation == HdInterpolationConstant) {
        if (_extAttrMapNeedUpdate) {
            // Apply a global lock to avoid race condition while doing parallel DG node evaluation.
            std::lock_guard<LockType> lock(dg_access_mutex);
            MAYAHYDRA_NS::GetExtensionAndDynamicAttributesFromNode(
                GetNode(),
                _extAttrNameToValueMap);
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

// Classify a plug's attribute as extension/dynamic (plugin-defined or user addAttr).
bool MayaHydraAdapter::IsExtensionOrDynamicAttribute(const MPlug& plug)
{
    MStatus status;
    MObject attrObj = plug.attribute(&status);
    if (!status) {
        return false;
    }
    MFnAttribute fnAttr(attrObj);
    return fnAttr.isExtension() || fnAttr.isDynamic();
}

bool MayaHydraAdapter::AttributeMessageAffectsExtensionPrimvars(MNodeMessage::AttributeMessage msg)
{
    const MNodeMessage::AttributeMessage mask = static_cast<MNodeMessage::AttributeMessage>(
        MNodeMessage::kAttributeSet | MNodeMessage::kAttributeRemoved | MNodeMessage::kAttributeAdded
        | MNodeMessage::kAttributeRenamed);
    return (msg & mask) != 0;
}

// Normalize the plug, filter non extension/dynamic attrs, then mark primvars dirty if allowed.
void MayaHydraAdapter::MaybeMarkPrimvarDirtyForAttributeChange(const MPlug& plug)
{
    if (ShouldSkipHydraUpdates(_mayaHydraSceneIndex)) {
        return;
    }
    // Trace to top-level plug: when a compound/array element's child changes
    // (e.g. aiLookAt[0].child(0)), Maya may only fire for the child. We must
    // still mark primvars dirty so the scene browser refreshes when resetting
    // to default (primvar removal). Duplicate dirty notifications are idempotent.
    MPlug topPlug = MayaHydra::GetTopPlug(plug);
    MStatus attrStatus;
    topPlug.attribute(&attrStatus);
    if (!attrStatus) {
        // During kAttributeRemoved the plug may no longer resolve; still invalidate primvar cache.
        MarkPrimvarDirtyForAttributeChange(topPlug);
        return;
    }
    if (!IsExtensionOrDynamicAttribute(topPlug)) {
        return;
    }
    if (ShouldMarkPrimvarDirtyForAttributeChange(topPlug)) {
        MarkPrimvarDirtyForAttributeChange(topPlug);
    }
}

// Marks primvars dirty when an attribute changes that affects primvar data. This includes:
// extension attributes (plugin-defined on node types) and dynamic attributes (user addAttr).
// Mark primvars dirty for an extension/dynamic attribute change and coalesce extra bits.
void MayaHydraAdapter::MarkPrimvarDirtyForAttributeChange(const MPlug& plug)
{
    if (ShouldSkipHydraUpdates(_mayaHydraSceneIndex)) {
        return;
    }
    MStatus status;
    MObject attrObj = plug.attribute(&status);
    if (!status) {
        // Plug may be invalid during kAttributeRemoved; still refresh cached extension/dynamic map.
        // The attribute set itself changed (add/remove), so many/unknown primvars may change at
        // once: the broad primvars locator is appropriate here.
        _extAttrMapNeedUpdate = true;
        MayaHydra::DirtyNotifier notifier(this);
        notifier.dirtyPrimvars();
        _maybeDirtyExtComputationPrimvars(*this, notifier);
        AddExtraDirtyForPrimvarAttributeChange(notifier, plug);
        notifier.flush();
        return;
    }
    MFnAttribute fnAttr(attrObj);
    if (fnAttr.isExtension() || fnAttr.isDynamic()) {
        _extAttrMapNeedUpdate = true;
        // Extension/dynamic attributes are exposed as constant primvars; emit the broad
        // primvars locator (many/unknown primvars change). Type-specific adapters add their
        // own schema on top via AddExtraDirtyForPrimvarAttributeChange (e.g. lights add the
        // light schema locator).
        MayaHydra::DirtyNotifier notifier(this);
        notifier.dirtyPrimvars();
        _maybeDirtyExtComputationPrimvars(*this, notifier);
        AddExtraDirtyForPrimvarAttributeChange(notifier, plug);
        notifier.flush();
    }
}

PXR_NAMESPACE_CLOSE_SCOPE
