// Copyright 2026 Autodesk
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
//
// Implements MayaHydraCustomDagAdapter: the fallback adapter for unrecognized
// Maya plugin DAG nodes. When InsertDag() finds no light/camera/shape adapter
// for a plugin node, it creates one of these. The adapter exposes the node as
// a "mayaCustomDagNode" Hydra prim with its type name and all non-default
// attributes available through MayaHydraCustomNodeDataSource.
//

#include "customDagAdapter.h"

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <maya/MDagPath.h>
#include <maya/MDagPathArray.h>
#include <maya/MFnAttribute.h>
#include <maya/MFnDagNode.h>
#include <maya/MFnDependencyNode.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>

PXR_NAMESPACE_OPEN_SCOPE

using namespace MayaHydra;

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraCustomDagAdapter, TfType::Bases<MayaHydraDagAdapter>>();
}

namespace {

// Maya callback invoked when any attribute on the custom plugin node is modified,
// added, removed, or renamed. For value changes (kAttributeSet), fires a
// per-attribute dirty locator (mayaNode.mayaAttributes.<attrName>). For
// structural changes (add/remove/rename), dirties the parent locator
// (mayaNode.mayaAttributes) so downstream scene indices re-read the full
// dictionary.
void _CustomNodeAttrChanged(
    MNodeMessage::AttributeMessage msg,
    MPlug& plug,
    MPlug& /*otherPlug*/,
    void* clientData)
{
    if (!MayaHydraAdapter::AttributeMessageAffectsExtensionPrimvars(msg)) {
        return;
    }

    // Normalize compound/array child plugs to their top-level parent
    // (e.g. colorR -> color) so we dirty the correct attribute name.
    MPlug topPlug = MayaHydra::GetTopPlug(plug);

    // Message attributes are connection-only with no data value; skip them.
    MStatus attrStatus;
    MObject attrObj = topPlug.attribute(&attrStatus);
    if (!attrStatus || attrObj.isNull()) {
        // During kAttributeRemoved the plug may no longer resolve; dirty the
        // entire mayaAttributes container so the dictionary is re-read.
        auto* adapter = reinterpret_cast<MayaHydraCustomDagAdapter*>(clientData);
        HdDataSourceLocator locator(
            MayaHydraAdapterTokens->mayaNode,
            MayaHydraAdapterTokens->mayaAttributes);
        adapter->GetMayaHydraSceneIndex()->DirtyPrims(
            {{ adapter->GetID(), HdDataSourceLocatorSet(locator) }});
        return;
    }
    if (attrObj.apiType() == MFn::kMessageAttribute) {
        return;
    }

    MFnAttribute attr(attrObj);
    MString attrNameStr = attr.name();
    const char* name = attrNameStr.asChar();

    // Skip Maya built-in base class attributes (visibility, castsShadows,
    // objectColorRGB, worldPosition, etc.). These are never included in the
    // mayaAttributes dictionary by GetNonDefaultMayaAttributesFromNode(), so
    // dirtying them would cause a pointless re-read with no data change.
    if (IsBaseClassAttrName(name)) {
        return;
    }

    auto* adapter = reinterpret_cast<MayaHydraCustomDagAdapter*>(clientData);

    // Structural changes (add/remove/rename) affect the dictionary key set;
    // dirty the parent locator so downstream re-reads the full dictionary.
    if (msg & (MNodeMessage::kAttributeAdded | MNodeMessage::kAttributeRemoved
               | MNodeMessage::kAttributeRenamed)) {
        HdDataSourceLocator locator(
            MayaHydraAdapterTokens->mayaNode,
            MayaHydraAdapterTokens->mayaAttributes);
        adapter->GetMayaHydraSceneIndex()->DirtyPrims(
            {{ adapter->GetID(), HdDataSourceLocatorSet(locator) }});
        return;
    }

    TfToken attrName(name);
    HdDataSourceLocator locator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        attrName);
    adapter->GetMayaHydraSceneIndex()->DirtyPrims(
        {{ adapter->GetID(), HdDataSourceLocatorSet(locator) }});
}

// Visibility-only callback for the shape node itself. Unlike the base class
// _TransformNodeDirty which fires DirtyTransform for any non-visibility plug,
// this callback only reacts to visibility-related plugs. Plugin attribute
// changes are handled separately by _CustomNodeAttrChanged.
void _CustomShapePlugDirty(MObject& node, MPlug& plug, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraCustomDagAdapter*>(clientData);
    if (MayaHydraDagAdapter::IsVisibilityRelatedPlug(plug)) {
        // Shape node: visibility only — no co-dirty transform (unlike parent transforms).
        MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(adapter, /*coDirtyTransform=*/false);
    }
}

// Transform-dirty callback for parent transform nodes only. Replicates the
// base class _TransformNodeDirty behavior: visibility plugs fire
// DirtyVisibility, all other plugs fire DirtyTransform.
void _CustomParentTransformDirty(MObject& node, MPlug& plug, void* clientData)
{
    TF_UNUSED(node);
    auto* adapter = reinterpret_cast<MayaHydraCustomDagAdapter*>(clientData);
    if (MayaHydraDagAdapter::IsVisibilityRelatedPlug(plug)) {
        MayaHydraDagAdapter::DirtyVisibilityRelatedPlug(adapter);
    } else {
        MayaHydraDagAdapter::DirtyTransformIfVisible(adapter);
    }
}

} // namespace

MayaHydraCustomDagAdapter::MayaHydraCustomDagAdapter(
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MDagPath& dagPath)
    : MayaHydraDagAdapter(
        mayaHydraSceneIndex->GetPrimPath(dagPath, false),
        mayaHydraSceneIndex,
        dagPath)
{
    MFnDependencyNode depNode(dagPath.node());
    _mayaTypeName = TfToken(depNode.typeName().asChar());
}

bool MayaHydraCustomDagAdapter::IsSupported() const
{
    return true;
}

void MayaHydraCustomDagAdapter::Populate()
{
    GetMayaHydraSceneIndex()->InsertPrim(
        this,
        MayaHydraAdapterTokens->mayaCustomDagNode,
        GetID());
    _isPopulated = true;
}

void MayaHydraCustomDagAdapter::CreateCallbacks()
{
    MStatus status;

    // Per-attribute dirty callback on the shape node.
    auto obj = GetNode();
    auto cbId = MNodeMessage::addAttributeChangedCallback(
        obj, _CustomNodeAttrChanged, this, &status);
    if (status) {
        AddCallback(cbId);
    }

    // Visibility-only plug-dirty callback on the shape node. We do NOT use
    // the base class _TransformNodeDirty here because it fires DirtyTransform
    // for every non-visibility plug change (e.g. intensity), which is wrong
    // for plugin attribute changes that we handle via _CustomNodeAttrChanged.
    cbId = MNodeMessage::addNodeDirtyPlugCallback(
        obj, _CustomShapePlugDirty, this, &status);
    if (status) {
        AddCallback(cbId);
    }

    // Walk the DAG hierarchy ABOVE the shape (parent transforms only) and
    // register transform-dirty callbacks. This mirrors what the base class
    // MayaHydraDagAdapter::CreateCallbacks() does, but skips the shape node.
    MDagPathArray dags;
    if (MDagPath::getAllPathsTo(GetDagPath().node(), dags)) {
        for (unsigned int i = 0; i < dags.length(); ++i) {
            auto dag = dags[i];
            _AddHierarchyChangedCallbacks(dag);
            dag.pop(); // skip the shape node
            for (; dag.length() > 0; dag.pop()) {
                MObject parentObj = dag.node();
                if (parentObj != MObject::kNullObj) {
                    cbId = MNodeMessage::addNodeDirtyPlugCallback(
                        parentObj, _CustomParentTransformDirty, this, &status);
                    if (status) {
                        AddCallback(cbId);
                    }
                    _AddHierarchyChangedCallbacks(dag);
                }
            }
        }
    }

    MayaHydraAdapter::CreateCallbacks();
}

void MayaHydraCustomDagAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

bool MayaHydraCustomDagAdapter::GetVisible()
{
    UpdateVisibility();
    return IsVisible(false);
}

VtDictionary MayaHydraCustomDagAdapter::GetNonDefaultMayaAttributes() const
{
    MayaHydra::DgAccessLock dgLock;

    VtDictionary attrs;
    GetNonDefaultMayaAttributesFromNode(GetNode(), attrs);
    return attrs;
}

PXR_NAMESPACE_CLOSE_SCOPE
