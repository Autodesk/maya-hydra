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
// Implements MayaHydraGenericDagAdapter: the fallback adapter for unrecognized
// Maya plugin DAG nodes. When InsertDag() finds no light/camera/shape adapter
// for a plugin node, it creates one of these. The adapter exposes the node as
// a "mayaCustomDagNode" Hydra prim with its type name and all non-default
// attributes available through MayaHydraGenericNodeDataSource.
//

#include "genericDagAdapter.h"

#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/mixedUtils.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/dataSourceLocator.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/xformSchema.h>

#include <mayaHydraLib/adapters/mayaAttrs.h>

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
    TfType::Define<MayaHydraGenericDagAdapter, TfType::Bases<MayaHydraDagAdapter>>();
}

namespace {

// Maya callback invoked when any attribute on the generic node is modified.
// Fires a per-attribute dirty locator (mayaNode.mayaAttributes.<attrName>)
// so downstream scene indices can react to specific attribute changes.
void _GenericNodeAttrChanged(
    MNodeMessage::AttributeMessage msg,
    MPlug& plug,
    MPlug& /*otherPlug*/,
    void* clientData)
{
    if (!(msg & MNodeMessage::kAttributeSet)) {
        return;
    }
    if (plug.isChild()) {
        return;
    }
    // Message attributes are connection-only with no data value; skip them.
    if (plug.attribute().apiType() == MFn::kMessageAttribute) {
        return;
    }

    MFnAttribute attr(plug.attribute());
    MString attrNameStr = attr.name();
    const char* name = attrNameStr.asChar();

    // Skip Maya built-in base class attributes (visibility, castsShadows,
    // objectColorRGB, worldPosition, etc.). These are never included in the
    // mayaAttributes dictionary by GetNonDefaultMayaAttributesFromNode(), so
    // dirtying them would cause a pointless re-read with no data change.
    if (IsBaseClassAttrName(name)) {
        return;
    }

    auto* adapter = reinterpret_cast<MayaHydraGenericDagAdapter*>(clientData);

    TfToken attrName(name);
    HdDataSourceLocator locator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        attrName);
    adapter->GetMayaHydraSceneIndex()->DirtyPrims({{ adapter->GetID(), HdDataSourceLocatorSet(locator) }});
}

// Visibility-only callback for the shape node itself. Unlike the base class
// _TransformNodeDirty which fires DirtyTransform for any non-visibility plug,
// this callback only reacts to visibility-related plugs. Plugin attribute
// changes are handled separately by _GenericNodeAttrChanged.
void _GenericShapePlugDirty(MObject& node, MPlug& plug, void* clientData)
{
    auto* adapter = reinterpret_cast<MayaHydraGenericDagAdapter*>(clientData);
    if (plug == MayaAttrs::dagNode::visibility
        || plug == MayaAttrs::dagNode::intermediateObject
        || plug == MayaAttrs::dagNode::overrideEnabled
        || plug == MayaAttrs::dagNode::overrideVisibility) {
        adapter->MarkDirty(HdChangeTracker::DirtyVisibility);
    }
}

// Transform-dirty callback for parent transform nodes only. Replicates the
// base class _TransformNodeDirty behavior: visibility plugs fire
// DirtyVisibility, all other plugs fire DirtyTransform.
void _GenericParentTransformDirty(MObject& node, MPlug& plug, void* clientData)
{
    auto* adapter = reinterpret_cast<MayaHydraGenericDagAdapter*>(clientData);
    if (plug == MayaAttrs::dagNode::visibility
        || plug == MayaAttrs::dagNode::intermediateObject
        || plug == MayaAttrs::dagNode::overrideEnabled
        || plug == MayaAttrs::dagNode::overrideVisibility) {
        if (adapter->IsVisible(false)) {
            adapter->InvalidateTransform();
            adapter->MarkDirty(
                HdChangeTracker::DirtyVisibility | HdChangeTracker::DirtyTransform);
        } else {
            adapter->MarkDirty(HdChangeTracker::DirtyVisibility);
        }
    } else if (adapter->IsVisible(false)) {
        adapter->InvalidateTransform();
        adapter->MarkDirty(HdChangeTracker::DirtyTransform);
    }
}

} // namespace

MayaHydraGenericDagAdapter::MayaHydraGenericDagAdapter(
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

bool MayaHydraGenericDagAdapter::IsSupported() const
{
    return true;
}

void MayaHydraGenericDagAdapter::Populate()
{
    GetMayaHydraSceneIndex()->InsertPrim(
        this,
        MayaHydraAdapterTokens->mayaCustomDagNode,
        GetID());
    _isPopulated = true;
}

void MayaHydraGenericDagAdapter::MarkDirty(HdDirtyBits dirtyBits)
{
    if (dirtyBits == 0) {
        return;
    }

    HdDataSourceLocatorSet locators;
    if (dirtyBits & HdChangeTracker::DirtyTransform) {
        locators.append(HdXformSchema::GetDefaultLocator());
    }
    if (dirtyBits & HdChangeTracker::DirtyVisibility) {
        locators.append(HdVisibilitySchema::GetDefaultLocator());
    }

    if (!locators.IsEmpty()) {
        GetMayaHydraSceneIndex()->DirtyPrims({{ GetID(), locators }});
    }
}

void MayaHydraGenericDagAdapter::CreateCallbacks()
{
    MStatus status;

    // Per-attribute dirty callback on the shape node.
    auto obj = GetNode();
    auto cbId = MNodeMessage::addAttributeChangedCallback(
        obj, _GenericNodeAttrChanged, this, &status);
    if (status) {
        AddCallback(cbId);
    }

    // Visibility-only plug-dirty callback on the shape node. We do NOT use
    // the base class _TransformNodeDirty here because it fires DirtyTransform
    // for every non-visibility plug change (e.g. intensity), which is wrong
    // for plugin attribute changes that we handle via _GenericNodeAttrChanged.
    cbId = MNodeMessage::addNodeDirtyPlugCallback(
        obj, _GenericShapePlugDirty, this, &status);
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
                        parentObj, _GenericParentTransformDirty, this, &status);
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

void MayaHydraGenericDagAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

bool MayaHydraGenericDagAdapter::GetVisible()
{
    UpdateVisibility();
    return IsVisible(false);
}

VtDictionary MayaHydraGenericDagAdapter::GetNonDefaultMayaAttributes() const
{
    VtDictionary attrs;
    GetNonDefaultMayaAttributesFromNode(GetNode(), attrs);
    return attrs;
}

PXR_NAMESPACE_CLOSE_SCOPE
