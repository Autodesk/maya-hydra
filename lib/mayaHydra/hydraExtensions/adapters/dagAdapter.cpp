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
#include "dagAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <mayaHydraLib/adapters/mhDirtyNotifier.h>

#include <pxr/base/gf/interval.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/tokens.h>

#include <maya/MAnimControl.h>
#include <maya/MDGContext.h>
#include <maya/MDGContextGuard.h>
#include <maya/MDagMessage.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnDagNode.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MTransformationMatrix.h>

PXR_NAMESPACE_OPEN_SCOPE
// Bring the MayaHydra namespace into scope.
// The following code currently lives inside the pxr namespace, but it would make more sense to 
// have it inside the MayaHydra namespace. This using statement allows us to use MayaHydra symbols
// from within the pxr namespace as if we were in the MayaHydra namespace.
// Remove this once the code has been moved to the MayaHydra namespace.
using namespace MayaHydra;

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraDagAdapter, TfType::Bases<MayaHydraAdapter>>();
}

// clang-format off
TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    (translate)
    (rotate)
    (scale)
    (instanceTransform)
    (instancer)
);
// clang-format on

namespace {

bool _IsNodeOnDagPath(const MObject& node, const MDagPath& path)
{
    if (node.isNull() || !path.isValid()) {
        return false;
    }
    for (MDagPath dagPath = path; dagPath.length() > 0; dagPath.pop()) {
        if (dagPath.node() == node) {
            return true;
        }
    }
    return false;
}

void _DirtyInstancerPlugs(MayaHydraDagAdapter* adapter)
{
    // Dirty the rprim: its instance index and the instanceTransform primvar changed.
    {
        MayaHydra::DirtyNotifier(adapter).dirtyInstancer().dirtyPrimvars();
    }
    // Dirty the instancer prim itself: topology and primvars (instanceTransform) changed.
    const SdfPath instancerId = adapter->GetInstancerID();
    if (!instancerId.IsEmpty()) {
        Fvp::DirtyNotifier notifier(*adapter->GetMayaHydraSceneIndex(), instancerId);
        notifier.dirtyInstancer().dirtyPrimvars();
    }
}

void _DirtyInstancerVisibilityPlug(MayaHydraDagAdapter* adapter, const MPlug& plug)
{
    // Per-instance visibility is expressed via instance indices / instanceTransform primvars,
    // not the prototype rprim visibility schema (which reflects the master DAG path only).
    _DirtyInstancerPlugs(adapter);
    if (_IsNodeOnDagPath(plug.node(), adapter->GetDagPath())) {
        MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(adapter);
    }
}

void _InstancerNodeDirty(MObject& node, MPlug& plug, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    adapter->RefreshInstancingState();
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_DAG_PLUG_DIRTY)
        .Msg(
            "Dag instancer adapter marking prim (%s) dirty because %s plug was "
            "dirtied.\n",
            adapter->GetID().GetText(),
            plug.partialName().asChar());
    if (MayaHydraDagAdapter::IsVisibilityRelatedPlug(plug)) {
        if (adapter->IsInstanced()) {
            _DirtyInstancerVisibilityPlug(adapter, plug);
        } else {
            MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(adapter);
        }
        return;
    }
    _DirtyInstancerPlugs(adapter);
}

void _TransformNodeDirty(MObject& node, MPlug& plug, void* clientData)
{
    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    // Adapters created before duplicateInstanced keep _TransformNodeDirty registered on what is
    // now a shared DAG node. Delegate to the instancer dirty policy once the node is instanced.
    //
    // Use the cached IsInstanced() state rather than querying MDagPath::getAllPathsTo() here:
    // that query is expensive and unsafe to run synchronously on every single dirty-plug
    // notification (this callback fires for every plug change on every ancestor path). The
    // transition itself is already detected cheaply and reliably via the dedicated
    // instance-added DAG message (see MayaHydraSceneIndex::AddNewInstance()), which calls
    // RebuildAdapterOnIdle(RebuildFlagCallbacks) to swap this callback for _InstancerNodeDirty
    // the next time adapters are rebuilt on idle.
    if (adapter->IsInstanced()) {
        _InstancerNodeDirty(node, plug, clientData);
        return;
    }
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_DAG_PLUG_DIRTY)
        .Msg(
            "Dag adapter marking prim (%s) dirty because .%s plug was "
            "dirtied.\n",
            adapter->GetID().GetText(),
            plug.partialName().asChar());
    if (MayaHydraDagAdapter::IsVisibilityRelatedPlug(plug)) {
        MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(adapter);
    } else {
        MayaHydraDagAdapter::DirtyTransformIfVisible(adapter);
    }
}

void _HierarchyChanged(MDagPath& child, MDagPath& parent, void* clientData)
{
    TF_UNUSED(child);
    auto* adapter = reinterpret_cast<MayaHydraDagAdapter*>(clientData);
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_DAG_HIERARCHY)
        .Msg(
            "Dag hierarchy changed for prim (%s) because %s had parent %s "
            "added/removed.\n",
            adapter->GetID().GetText(),
            child.partialPathName().asChar(),
            parent.partialPathName().asChar());
    adapter->RemoveCallbacks();
    adapter->RemovePrim();
    adapter->GetMayaHydraSceneIndex()->RecreateAdapterOnIdle(adapter->GetID(), adapter->GetNode());
}

const auto _instancePrimvarDescriptors = HdPrimvarDescriptorVector {
    { _tokens->instanceTransform, HdInterpolationInstance, HdPrimvarRoleTokens->none },
};

} // namespace

// MayaHydraDagAdapter is the adapter base class for any dag object.
MayaHydraDagAdapter::MayaHydraDagAdapter(
    const SdfPath&        id,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MDagPath&       dagPath)
    : MayaHydraAdapter(dagPath.node(), id, mayaHydraSceneIndex)
    , _dagPath(dagPath)
{
    // We shouldn't call virtual functions in constructors.
    _isVisible = GetDagPath().isVisible();
    _visibilityDirty = false;
    _isInstanced = _dagPath.isInstanced() && _dagPath.instanceNumber() == 0;
}

GfMatrix4d MayaHydraDagAdapter::GetTransform()
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg(
            "Called MayaHydraDagAdapter::GetTransform() - %s\n",
            _dagPath.partialPathName().asChar());

    if (_invalidTransform) {
        if (IsInstanced()) {
            _transform.SetIdentity();
        } else {
            _transform = GetGfMatrixFromMaya(_dagPath.inclusiveMatrix());
        }
        _invalidTransform = false;
    }

    return _transform;
}

size_t
MayaHydraDagAdapter::SampleTransform(size_t maxSampleCount, float* times, GfMatrix4d* samples)
{
    return GetMayaHydraSceneIndex()->SampleValues(maxSampleCount, times, samples, [&]() -> GfMatrix4d {
        return GetGfMatrixFromMaya(_dagPath.inclusiveMatrix());
    });
}

void MayaHydraDagAdapter::RefreshInstancingState()
{
    MDagPathArray dags;
    if (MDagPath::getAllPathsTo(GetDagPath().node(), dags)) {
        _isInstanced = dags.length() > 1;
    }
}

void MayaHydraDagAdapter::CreateCallbacks()
{
    MStatus status;

    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("Creating dag adapter callbacks for prim (%s).\n", GetID().GetText());

    MDagPathArray dags;
    if (MDagPath::getAllPathsTo(GetDagPath().node(), dags)) {
        const auto numDags = dags.length();
        const bool instanced = numDags > 1;
        // Refresh cached instancing state: adapters created before duplicateInstanced
        // must pick up instancer dirty policy when callbacks are rebuilt.
        _isInstanced = instanced;
        auto       dagNodeDirtyCallback = instanced ? _InstancerNodeDirty : _TransformNodeDirty;
        for (auto i = decltype(numDags) { 0 }; i < numDags; ++i) {
            auto dag = dags[i];
            for (; dag.length() > 0; dag.pop()) {
                MObject obj = dag.node();
                if (obj != MObject::kNullObj) {
                    auto id = MNodeMessage::addNodeDirtyPlugCallback(
                        obj, dagNodeDirtyCallback, this, &status);
                    if (status) {
                        AddCallback(id);
                    }
                    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
                        .Msg(
                            "- Added %s callback for dagPath (%s).\n",
                            (instanced ? "_InstancerNodeDirty" : "_TransformNodeDirty"),
                            dag.partialPathName().asChar());
                    _AddHierarchyChangedCallbacks(dag);
                }
            }
        }
    }
    MayaHydraAdapter::CreateCallbacks();
}

void MayaHydraDagAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    // Instancing is expressed via instancedBy / instancerTopology locators on the prototype
    // prim (see DirtyNotifier::dirtyInstancer). MayaHydra never calls HdRenderIndex::
    // InsertInstancer, so RemoveInstancer must not run here — it crashes on teardown when
    // duplicate -ilf correctly sets _isInstanced.
    _isPopulated = false;
    // Note: _isRprimResolved is reset automatically by IsRprimTypeSupportedForPrim() whenever
    // _isPopulated is false (see MayaHydraAdapter::IsRprimTypeSupportedForPrim). No explicit
    // reset needed here.
}

bool MayaHydraDagAdapter::UpdateVisibility()
{
    if (ARCH_UNLIKELY(!GetDagPath().isValid())) {
        return false;
    }
    const auto visible = _GetVisibility();
    _visibilityDirty = false;
    if (visible != _isVisible) {
        _isVisible = visible;
        return true;
    }
    return false;
}

bool MayaHydraDagAdapter::IsVisible(bool checkDirty)
{
    if (checkDirty && _visibilityDirty) {
        UpdateVisibility();
    }
    return _isVisible;
}

VtIntArray MayaHydraDagAdapter::GetInstanceIndices(const SdfPath& prototypeId)
{
    if (!IsInstanced()) {
        return {};
    }
    MDagPathArray dags;
    if (!MDagPath::getAllPathsTo(GetDagPath().node(), dags)) {
        return {};
    }
    const auto numDags = dags.length();
    VtIntArray ret;
    ret.reserve(numDags);
    for (auto i = decltype(numDags) { 0 }; i < numDags; ++i) {
        if (dags[i].isValid() && dags[i].isVisible()) {
            ret.push_back(static_cast<int>(ret.size()));
        }
    }
    return ret;
}

void MayaHydraDagAdapter::_AddHierarchyChangedCallbacks(MDagPath& dag)
{
    MStatus status;
    auto    id = MDagMessage::addParentAddedDagPathCallback(dag, _HierarchyChanged, this, &status);
    if (status) {
        AddCallback(id);
    }
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("- (%s) added parent added callback for inclusive ancestor (%s).\n", _dagPath.partialPathName().asChar(), dag.partialPathName().asChar());

    // We need a parent removed callback, even for non-instances,
    // because when an object is removed from the scene due to an
    // undo, no pre-removal (or about-to-delete, or destroyed)
    // callbacks are triggered. The parent-removed callback IS
    // triggered, though, so it's a way to catch deletion due to
    // undo...
    id = MDagMessage::addParentRemovedDagPathCallback(dag, _HierarchyChanged, this, &status);
    if (status) {
        AddCallback(id);
    }
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
        .Msg("- (%s) added parent removed callback for inclusive ancestor (%s).\n", _dagPath.partialPathName().asChar(), dag.partialPathName().asChar());
}

SdfPath MayaHydraDagAdapter::GetInstancerID() const
{
    MDagPathArray dags;
    if (!MDagPath::getAllPathsTo(GetDagPath().node(), dags) || dags.length() <= 1) {
        return {};
    }

    return GetID().AppendProperty(_tokens->instancer);
}

HdPrimvarDescriptorVector
MayaHydraDagAdapter::GetInstancePrimvarDescriptors(HdInterpolation interpolation) const
{
    if (interpolation == HdInterpolationInstance) {
        return _instancePrimvarDescriptors;
    } else {
        return {};
    }
}

bool MayaHydraDagAdapter::_GetVisibility() const { return GetDagPath().isVisible(); }

VtValue MayaHydraDagAdapter::GetInstancePrimvar(const TfToken& key)
{
    if (key == _tokens->instanceTransform) {
        MDagPathArray dags;
        if (!MDagPath::getAllPathsTo(GetDagPath().node(), dags)) {
            return {};
        }
        const auto          numDags = dags.length();
        VtArray<GfMatrix4d> ret;
        ret.reserve(numDags);
        for (auto i = decltype(numDags) { 0 }; i < numDags; ++i) {
            if (dags[i].isValid() && dags[i].isVisible()) {
                ret.push_back(GetGfMatrixFromMaya(dags[i].inclusiveMatrix()));
            }
        }
        return VtValue(ret);
    }
    return {};
}

bool MayaHydraDagAdapter::IsVisibilityRelatedPlug(const MPlug& plug)
{
    return plug == MayaAttrs::dagNode::visibility || plug == MayaAttrs::dagNode::intermediateObject
        || plug == MayaAttrs::dagNode::overrideEnabled
        || plug == MayaAttrs::dagNode::overrideVisibility;
}

void MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(MayaHydraDagAdapter* adapter, bool coDirtyTransform)
{
    // Can't query the new visibility here — the plug dirty hasn't propagated yet.
    // Mark _visibilityDirty so IsVisible() re-reads it on the next query.
    adapter->InvalidateVisibility();
    MayaHydra::DirtyNotifier notifier(adapter);
    if (coDirtyTransform && adapter->IsVisible(false)) {
        adapter->InvalidateTransform();
        notifier.dirtyTransform();
    }
    notifier.dirtyVisibility();
}

void MayaHydraDagAdapter::DirtyTransformIfVisible(MayaHydraDagAdapter* adapter)
{
    if (!adapter->IsVisible(false)) {
        return;
    }
    adapter->InvalidateTransform();
    MayaHydra::DirtyNotifier(adapter).dirtyTransform();
}

PXR_NAMESPACE_CLOSE_SCOPE
