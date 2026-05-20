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
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
#ifndef MAYAHYDRALIB_ADAPTER_H
#define MAYAHYDRALIB_ADAPTER_H

#include <mayaHydraLib/api.h>

#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/base/arch/hints.h>
#include <pxr/base/gf/interval.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/selection.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hdx/taskController.h>

#include <maya/MMessage.h>
#include <maya/MAnimControl.h>
#include <maya/MDGContextGuard.h>
#include <maya/MDrawContext.h>
#include <maya/MPointArray.h>
#include <maya/MSelectionContext.h>
#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MSelectionList.h>

#include <string>
#include <unordered_set>
#include <vector>
#include <map>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief MayaHydraAdapter is the base class for all adapters. An adapter is used to translate from
 * Maya data to hydra data.
 */

class MayaHydraAdapter
{
public:
    MAYAHYDRALIB_API
    /// Create an adapter for the given Maya node and prim path.
    MayaHydraAdapter(const MObject& node, const SdfPath& id, MayaHydraSceneIndex* mayaHydraSceneIndex);
    MAYAHYDRALIB_API
    /// Destroy the adapter and remove any registered callbacks.
    virtual ~MayaHydraAdapter();

    /// Return the Hydra prim path for this adapter.
    const SdfPath&       GetID() const { return _id; }
    /// Return the owning scene index.
    MayaHydraSceneIndex* GetMayaHydraSceneIndex() const { return _mayaHydraSceneIndex; }
    MAYAHYDRALIB_API
    /// Track a callback id for later removal.
    void AddCallback(MCallbackId callbackId);
    MAYAHYDRALIB_API
    /// Remove registered callbacks for this adapter.
    virtual void RemoveCallbacks();
    MAYAHYDRALIB_API
    /// Return a value for the requested Hydra data source key.
    virtual VtValue Get(const TfToken& key);
    /// Return the underlying Maya node.
    const MObject&  GetNode() const { return _node; }
    MAYAHYDRALIB_API
    /// Return whether the adapter can be created for this node.
    virtual bool IsSupported() const = 0;
    MAYAHYDRALIB_API
    /// Return whether this adapter supports the given Hydra type id.
    virtual bool HasType(const TfToken& typeId) const;
    MAYAHYDRALIB_API
    /// Return the visibility state for this prim.
    virtual bool GetVisible() { return true; }

    MAYAHYDRALIB_API
    /// Register Maya callbacks that dirty or invalidate this prim.
    virtual void CreateCallbacks();
    /// Mark the prim as dirty for the given bitset.
    virtual void MarkDirty(HdDirtyBits dirtyBits) = 0;
    /// Remove the prim from the scene index.
    virtual void RemovePrim() = 0;
    /// Insert the prim into the scene index.
    virtual void Populate() = 0;

    MAYAHYDRALIB_API
    /// Perform any one-time adapter initialization.
    static MStatus Initialize();

    /// Return whether the prim has been populated.
    bool IsPopulated() const { return _isPopulated; }

    // ---- Primvar dirtying flow (extension/dynamic attributes only) ----
    // MarkPrimvarDirtyForAttributeChange: final MarkDirty call (primvar + extra bits).
    // No-op if the plug is not an extension or dynamic attribute.
    // MaybeMarkPrimvarDirtyForAttributeChange: normalize plug + apply policy.
    // ShouldMarkPrimvarDirtyForAttributeChange: per-adapter gating to avoid duplicates.
    /// Final primvar dirtying step for an attribute change (primvar + consolidated bits).
    /// No-op if the plug is not an extension or dynamic attribute.
    void MarkPrimvarDirtyForAttributeChange(const MPlug& plug);

    /// Derived-class hook: override to add extra dirty bits for consolidation when a primvar-
    /// affecting attr changes (e.g. light param attrs need DirtyParams|DirtyShadowParams).
    /// Consolidating into one MarkDirty reduces redundant scene index notifications.
    virtual HdDirtyBits GetConsolidatedDirtyBitsForPrimvarAttributeChange(const MPlug& plug) const
    {
        return 0;
    }

    /// Call from attribute-changed callbacks when an extension/dynamic attr change should
    /// mark primvars dirty. Resolves \p plug to the appropriate parent/root plug before
    /// marking primvars dirty. This can still be invoked for child plugs of compound/array
    /// attributes (e.g. color.r), so callers may see multiple invocations that coalesce
    /// to the same top plug.
    /// Override ShouldMarkPrimvarDirtyForAttributeChange to return false when another callback
    /// already marks DirtyPrimvar (e.g. mesh _dirtyBits, light param attr list) to avoid
    /// duplicate notifications.
    /// Orchestrates primvar dirtying for an attribute change:
    /// - normalizes to the top plug (compound/array element safety),
    /// - filters non extension/dynamic attrs,
    /// - consults ShouldMarkPrimvarDirtyForAttributeChange before calling MarkPrimvarDirtyForAttributeChange.
    void MaybeMarkPrimvarDirtyForAttributeChange(const MPlug& plug);
    /// Policy gate for primvar dirtying; adapters override to suppress primvar dirty when
    /// another path already marks the right schema dirty bits (e.g. param attributes).
    /// If this returns false, MaybeMarkPrimvarDirtyForAttributeChange becomes a no-op for that plug.
    virtual bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const { return true; }

    // ---- Param-attribute helpers (schema dirtying vs primvar dirtying) ----
    // These three helpers are intentionally layered:
    // - IsParamAttribute: low-level predicate usable in multiple contexts.
    // - GetParamAttributeSet: cached set per static name array (no rebuild per call).
    // - ShouldMarkPrimvarDirtyForParamAttrs: convenience wrapper for the common gating pattern.
    /// Return true if the plug's attribute name is in the provided param set.
    static bool IsParamAttribute(const MPlug& plug, const std::unordered_set<std::string>& paramAttrs);

    template <size_t N>
    /// Build a cached attribute-name set from a static name array.
    static const std::unordered_set<std::string>& GetParamAttributeSet(const char* const (&names)[N])
    {
        static const std::unordered_set<std::string> s(names, names + N);
        return s;
    }

    /// Helper for adapters with param attribute lists. Use in ShouldMarkPrimvarDirtyForAttributeChange
    /// override: return ShouldMarkPrimvarDirtyForParamAttrs(plug, kXxxParamAttributeNames).
    template <size_t N>
    /// Convenience wrapper for the common "param attrs should not dirty primvars" policy.
    static bool ShouldMarkPrimvarDirtyForParamAttrs(const MPlug& plug, const char* const (&names)[N])
    {
        return !IsParamAttribute(plug, GetParamAttributeSet(names));
    }

    /// Return true if \p plug is an extension or dynamic attribute.
    static bool IsExtensionOrDynamicAttribute(const MPlug& plug);

    /// True for attribute-changed messages that should refresh extension/dynamic primvars: value
    /// changes (kAttributeSet) and DG structure (add/remove/rename).
    static bool AttributeMessageAffectsExtensionPrimvars(MNodeMessage::AttributeMessage msg);

    MAYAHYDRALIB_API
    /// Return the mesh topology for this prim (if applicable).
    virtual HdMeshTopology GetMeshTopology() { return {}; }
    MAYAHYDRALIB_API
    /// Return the basis curves topology for this prim (if applicable).
    virtual HdBasisCurvesTopology GetBasisCurvesTopology() { return {}; }
    MAYAHYDRALIB_API
    /// Return the OpenSubdiv tags (creases, corners, interpolation rules) for this prim
    /// when subdivision is active. Default returns empty tags.
    virtual PxOsdSubdivTags GetSubdivTags() { return {}; }
    MAYAHYDRALIB_API
    /// Return the Hydra render tag for this prim.
    virtual TfToken GetRenderTag() const { return TfToken(); }
    MAYAHYDRALIB_API
    /// Return the world transform for this prim.
    virtual GfMatrix4d GetTransform() { return GfMatrix4d(); }
    MAYAHYDRALIB_API
    /// Return primvar descriptors for a given interpolation.
    virtual HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation);
    MAYAHYDRALIB_API
    /// Return whether the prim should be treated as double-sided.
    virtual bool GetDoubleSided() const { return true; }
    MAYAHYDRALIB_API
    /// Return the cull style used by the prim.
    virtual HdCullStyle GetCullStyle() const { return HdCullStyleNothing; }
    MAYAHYDRALIB_API
    /// Return the display style (refine level and shading flags).
    virtual HdDisplayStyle GetDisplayStyle() { 
        constexpr int refineLevel = 0;
        constexpr bool flatShading = false;
        constexpr bool displacement = false;
        constexpr bool occludedSelectionShowsThrough = false;
        constexpr bool pointsShadingEnabled = false;
        constexpr bool materialIsFinal = false;
        return HdDisplayStyle(refineLevel, flatShading, displacement, occludedSelectionShowsThrough, pointsShadingEnabled, materialIsFinal); }
    MAYAHYDRALIB_API
    /// Return the bounding box for this prim.
    virtual GfBBox3d GetBoundingBox() { return GfBBox3d(); }
    MAYAHYDRALIB_API
    /// Return the display color for this prim.
    virtual GfVec4f GetDisplayColor() const { return {1.f,1.f,1.f,1.f}; }

    /// Return whether the prim emits light (used by light adapters).
    virtual bool Illuminated() const { return false; }

protected:
    SdfPath                  _id;
    std::vector<MCallbackId> _callbacks;
    MayaHydraSceneIndex*     _mayaHydraSceneIndex;
    MObject                  _node;
    VtDictionary             _extAttrNameToValueMap;
    bool                     _extAttrMapNeedUpdate { true };

    bool _isPopulated = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_ADAPTER_H
