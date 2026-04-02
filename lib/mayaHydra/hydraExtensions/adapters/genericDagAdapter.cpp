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
    MPlug& otherPlug,
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

    auto* adapter = reinterpret_cast<MayaHydraGenericDagAdapter*>(clientData);

    MFnAttribute attr(plug.attribute());
    TfToken attrName(attr.name().asChar());
    HdDataSourceLocator locator(
        MayaHydraAdapterTokens->mayaNode,
        MayaHydraAdapterTokens->mayaAttributes,
        attrName);
    adapter->GetMayaHydraSceneIndex()->DirtyPrims({{ adapter->GetID(), HdDataSourceLocatorSet(locator) }});
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
    auto obj = GetNode();
    auto id = MNodeMessage::addAttributeChangedCallback(
        obj, _GenericNodeAttrChanged, this, &status);
    if (status) {
        AddCallback(id);
    }
    MayaHydraDagAdapter::CreateCallbacks();
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
    return GetDagPath().isVisible();
}

VtDictionary MayaHydraGenericDagAdapter::GetNonDefaultMayaAttributes() const
{
    VtDictionary attrs;
    GetNonDefaultMayaAttributesFromNode(GetNode(), attrs);
    return attrs;
}

PXR_NAMESPACE_CLOSE_SCOPE
