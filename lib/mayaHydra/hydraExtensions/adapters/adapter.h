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
#include <maya/MSelectionList.h>

#include <vector>

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

    MAYAHYDRALIB_API
    virtual HdMeshTopology GetMeshTopology() { return {}; }
    MAYAHYDRALIB_API
    virtual HdBasisCurvesTopology GetBasisCurvesTopology() { return {}; }
    MAYAHYDRALIB_API
    virtual TfToken GetRenderTag() const { return TfToken(); }
    MAYAHYDRALIB_API
    virtual GfMatrix4d GetTransform() { return GfMatrix4d(); }
    MAYAHYDRALIB_API
    virtual HdPrimvarDescriptorVector GetPrimvarDescriptors(HdInterpolation interpolation)
    {
        return HdPrimvarDescriptorVector();
    }
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
    virtual GfBBox3d GetBoundingBox() const { return GfBBox3d(); }
    MAYAHYDRALIB_API
    virtual GfVec4f GetDisplayColor() const { return {1.f,1.f,1.f,1.f}; }

protected:
    SdfPath                  _id;
    std::vector<MCallbackId> _callbacks;
    MayaHydraSceneIndex*     _mayaHydraSceneIndex;
    MObject                  _node;

    bool _isPopulated = false;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_ADAPTER_H
