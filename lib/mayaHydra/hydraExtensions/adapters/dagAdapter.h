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
#ifndef MAYAHYDRALIB_DG_ADAPTER_H
#define MAYAHYDRALIB_DG_ADAPTER_H

#include <mayaHydraLib/adapters/adapter.h>
#include <mayaHydraLib/adapters/adapterDebugCodes.h>
#include <mayaHydraLib/mixedUtils.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range3d.h>
#include <pxr/base/tf/token.h>
#include <pxr/imaging/hd/meshTopology.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/sceneDelegate.h>
#include <pxr/pxr.h>

#include <maya/MBoundingBox.h>
#include <maya/MDagPath.h>
#include <maya/MFn.h>
#include <maya/MFnDagNode.h>
#include <maya/MMatrix.h>
#include <maya/MMessage.h>
#include <maya/MPlug.h>

#include <functional>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief MayaHydraDagAdapter is the adapter base class for any dag object.
 */
class MayaHydraDagAdapter : public MayaHydraAdapter
{
protected:
    MAYAHYDRALIB_API
    MayaHydraDagAdapter(const SdfPath& id, MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dagPath);

public:
    MAYAHYDRALIB_API
    virtual ~MayaHydraDagAdapter() = default;
    MAYAHYDRALIB_API
    virtual bool GetVisible() override { return IsVisible(); }
    MAYAHYDRALIB_API
    virtual void CreateCallbacks() override;
    MAYAHYDRALIB_API
    virtual void RemovePrim() override;
    MAYAHYDRALIB_API
    GfMatrix4d GetTransform() override;
    /// Refresh the cached world transform; returns whether it changed.
    MAYAHYDRALIB_API
    bool UpdateTransformIfChanged();
    MAYAHYDRALIB_API
    size_t SampleTransform(size_t maxSampleCount, float* times, GfMatrix4d* samples);
    MAYAHYDRALIB_API
    bool            UpdateVisibility();
    bool            IsVisible(bool checkDirty = true);
    const MDagPath& GetDagPath() const { return _dagPath; }
    void            InvalidateTransform() { _invalidTransform = true; }
    void            InvalidateVisibility() { _visibilityDirty = true; }
    bool            IsInstanced() const { return _isInstanced; }
    /// Re-read MDagPath::getAllPathsTo for this adapter's DAG node and update _isInstanced.
    MAYAHYDRALIB_API
    void RefreshInstancingState();
    MAYAHYDRALIB_API
    SdfPath GetInstancerID() const;
    MAYAHYDRALIB_API
    virtual VtIntArray GetInstanceIndices(const SdfPath& prototypeId);
    MAYAHYDRALIB_API
    HdPrimvarDescriptorVector GetInstancePrimvarDescriptors(HdInterpolation interpolation) const;
    MAYAHYDRALIB_API
    VtValue GetInstancePrimvar(const TfToken& key);

    bool Illuminated() const override { return (MFnDependencyNode(_dagPath.node()).typeName().asChar() == TfToken("mesh")); }

    /// True for visibility, intermediateObject, overrideEnabled, overrideVisibility.
    static bool IsVisibilityRelatedPlug(const MPlug& plug);

    /// Invalidate visibility cache and emit visibility locators. When \p coDirtyTransform is true
    /// and the prim is currently visible, also invalidate transform and dirty the transform
    /// locator (the plug dirty has not propagated yet — use IsVisible(false)).
    static void DirtyVisibilityRelatedPlug(MayaHydraDagAdapter* adapter, bool coDirtyTransform = true);

    /// When the prim is visible, invalidate transform and emit the transform locator.
    static void DirtyTransformIfVisible(MayaHydraDagAdapter* adapter);

protected:
    MAYAHYDRALIB_API
    void _AddHierarchyChangedCallbacks(MDagPath& dag);
    MAYAHYDRALIB_API
    virtual bool _GetVisibility() const;

private:
    MDagPath   _dagPath;
    GfMatrix4d _transform;
    bool       _isVisible = true;
    bool       _visibilityDirty = true;
    bool       _invalidTransform = true;
    bool       _isInstanced = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_DG_ADAPTER_H
