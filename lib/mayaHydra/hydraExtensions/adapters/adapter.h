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
    MayaHydraAdapter(const MObject& node, const SdfPath& id, MayaHydraSceneIndex* mayaHydraSceneIndex);
    MAYAHYDRALIB_API
    virtual ~MayaHydraAdapter();

    const SdfPath&       GetID() const { return _id; }
    MayaHydraSceneIndex* GetMayaHydraSceneIndex() const { return _mayaHydraSceneIndex; }
    MAYAHYDRALIB_API
    void AddCallback(MCallbackId callbackId);
    MAYAHYDRALIB_API
    virtual void RemoveCallbacks();
    MAYAHYDRALIB_API
    virtual VtValue Get(const TfToken& key);
    const MObject&  GetNode() const { return _node; }
    MAYAHYDRALIB_API
    virtual bool IsSupported() const = 0;
    MAYAHYDRALIB_API
    virtual bool HasType(const TfToken& typeId) const;
    MAYAHYDRALIB_API
    virtual bool GetVisible() { return true; }

    MAYAHYDRALIB_API
    virtual void CreateCallbacks();
    virtual void MarkDirty(HdDirtyBits dirtyBits) = 0;
    virtual void RemovePrim() = 0;
    virtual void Populate() = 0;

    MAYAHYDRALIB_API
    static MStatus Initialize();

    bool IsPopulated() const { return _isPopulated; }

    void MarkPrimvarDirtyForAttributeChange(const MPlug& plug);

    /// Override to add extra dirty bits when a primvar-affecting attr changes (e.g. light param
    /// attrs need DirtyParams|DirtyShadowParams). Consolidating into one MarkDirty reduces
    /// redundant scene index notifications.
    virtual HdDirtyBits GetExtraDirtyBitsForPrimvarAttributeChange(const MPlug& plug) const
    {
        return 0;
    }

    /// Call from attribute-changed callbacks when an attr change should mark primvars dirty.
    /// Skips child plugs to avoid duplicate notifications for compound attrs (e.g. color.r).
    /// Override ShouldMarkPrimvarDirtyForAttributeChange to return false when another callback
    /// already marks DirtyPrimvar (e.g. mesh _dirtyBits, light param attr list).
    void MaybeMarkPrimvarDirtyForAttributeChange(const MPlug& plug);
    virtual bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const { return true; }

    /// Helper for adapters with param attribute lists. Returns true if plug's attribute is in the set.
    /// Use: ShouldMarkPrimvarDirtyForAttributeChange returns !IsParamAttribute(plug, paramAttrs).
    static bool IsParamAttribute(const MPlug& plug, const std::unordered_set<std::string>& paramAttrs);

    /// Build a static param attribute set from an array. Each instantiation caches its own set.
    template <size_t N>
    static const std::unordered_set<std::string>& GetParamAttributeSet(const char* const (&names)[N])
    {
        static std::unordered_set<std::string> s;
        static bool init = false;
        if (!init) {
            for (const char* name : names) {
                s.insert(name);
            }
            init = true;
        }
        return s;
    }

    /// Helper for adapters with param attribute lists. Use in ShouldMarkPrimvarDirtyForAttributeChange
    /// override: return ShouldMarkPrimvarDirtyForParamAttrs(plug, kXxxParamAttributeNames).
    template <size_t N>
    static bool ShouldMarkPrimvarDirtyForParamAttrs(const MPlug& plug, const char* const (&names)[N])
    {
        return !IsParamAttribute(plug, GetParamAttributeSet(names));
    }

    /// Return the top-level plug for a child/array element plug.
    /// (e.g. aiLookAt[0].child(0) -> aiLookAt)
    static MPlug GetTopPlug(const MPlug& plug);

    /// When true, all non-builtin attributes are translated to primvars (e.g. for lights).
    /// When false, only extension and dynamic attributes. Default is false.
    virtual bool IncludeAllAttributesInPrimvars() const { return false; }

    /// Returns true if \p plug is an extension or dynamic attribute. Used by adapters
    /// to route extension/dynamic attribute changes to HandleExtensionAndDynamicAttributesDirty
    /// instead of marking schema params dirty.
    static bool IsExtensionOrDynamicAttribute(const MPlug& plug);
    /// Mark primvars dirty for extension or dynamic attributes without touching schema params.
    void HandleExtensionAndDynamicAttributesDirty(const MPlug& plug);

    MAYAHYDRALIB_API
    virtual HdMeshTopology GetMeshTopology() { return {}; }
    MAYAHYDRALIB_API
    virtual HdBasisCurvesTopology GetBasisCurvesTopology() { return {}; }
    MAYAHYDRALIB_API
    virtual TfToken GetRenderTag() const { return TfToken(); }
    MAYAHYDRALIB_API
    virtual GfMatrix4d GetTransform() { return GfMatrix4d(); }
    MAYAHYDRALIB_API
    virtual HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation);
    MAYAHYDRALIB_API
    virtual bool GetDoubleSided() const { return true; }
    MAYAHYDRALIB_API
    virtual HdCullStyle GetCullStyle() const { return HdCullStyleNothing; }
    MAYAHYDRALIB_API
    virtual HdDisplayStyle GetDisplayStyle() { 
        constexpr int refineLevel = 0;
        constexpr bool flatShading = false;
        constexpr bool displacement = false;
        constexpr bool occludedSelectionShowsThrough = false;
        constexpr bool pointsShadingEnabled = false;
        constexpr bool materialIsFinal = false;
        return HdDisplayStyle(refineLevel, flatShading, displacement, occludedSelectionShowsThrough, pointsShadingEnabled, materialIsFinal); }
    MAYAHYDRALIB_API
    virtual GfBBox3d GetBoundingBox() { return GfBBox3d(); }
    MAYAHYDRALIB_API
    virtual GfVec4f GetDisplayColor() const { return {1.f,1.f,1.f,1.f}; }

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
