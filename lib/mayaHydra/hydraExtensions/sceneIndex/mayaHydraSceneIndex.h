//
// Copyright 2023 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRASCENEINDEX_H
#define MAYAHYDRASCENEINDEX_H

#include <maya/MDagPath.h>
#include <maya/MFrameContext.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <maya/MViewport2Renderer.h>
#include <maya/MDrawContext.h>

#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/materialAdapter.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/cameraAdapter.h>
#include <mayaHydraLib/sceneIndex/mayaHydraDefaultLightDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraMaterialDataSource.h>

#include "flowViewport/sceneIndex/fvpPathInterface.h"

#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/selection.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include "pxr/imaging/hd/dirtyBitsTranslator.h"

#include <unordered_map>

namespace FVP_NS_DEF {
class RenderIndexProxy;
}

PXR_NAMESPACE_OPEN_SCOPE

struct MayaHydraInitData
{
    MayaHydraInitData(
        TfToken            nameIn,
        HdEngine&          engineIn,
        HdRenderIndex*     renderIndexIn,
        HdRendererPlugin*  rendererPluginIn,
        HdxTaskController* taskControllerIn,
        const SdfPath&     delegateIDIn,
        bool               isHdStIn)
        : name(nameIn)
        , engine(engineIn)
        , renderIndex(renderIndexIn)
        , rendererPlugin(rendererPluginIn)
        , taskController(taskControllerIn)
        , delegateID(delegateIDIn)
        , isHdSt(isHdStIn)
    {
    }

    TfToken            name;
    HdEngine&          engine;
    HdRenderIndex*     renderIndex;
    HdRendererPlugin*  rendererPlugin;
    HdxTaskController* taskController;
    SdfPath            delegateID;
    bool               isHdSt;
};

class MayaHydraSceneIndex;
TF_DECLARE_WEAK_AND_REF_PTRS(MayaHydraSceneIndex);
/**
 * \brief MayaHydraSceneIndex is a scene index to produce the hydra scene from Maya native scene.
 */
class MAYAHYDRALIB_API MayaHydraSceneIndex : public HdRetainedSceneIndex, public Fvp::PathInterface
{
public:
    enum RebuildFlags : uint32_t
    {
        RebuildFlagPrim = 1 << 1,
        RebuildFlagCallbacks = 1 << 2,
    };
    template <typename T> using AdapterMap = std::unordered_map<SdfPath, T, SdfPath::Hash>;

    static MayaHydraSceneIndexRefPtr New(
        MayaHydraInitData& initData,
        bool lightEnabled) {
        return TfCreateRefPtr(new MayaHydraSceneIndex(initData, lightEnabled));
    }

    ~MayaHydraSceneIndex();

    //Call this before the destructor is called.
    void RemoveCallbacksAndDeleteAdapters();

    // ------------------------------------------------------------------------
    // Maya Hydra scene producer implementations
    // Propogate scene changes from Maya to Hydra
    void HandleCompleteViewportScene(const MDataServerOperation::MViewportScene& scene, MFrameContext::DisplayStyle ds);

    // Populate data from Maya
    void Populate();

    // Add hydra pick points and items to Maya's selection list
    bool AddPickHitToSelectionList(
        const HdxPickHit& hit,
        const MHWRender::MSelectionInfo& selectInfo,
        MSelectionList& selectionList,
        MPointArray& worldSpaceHitPts);

    bool IsPickedNodeInComponentsPickingMode(const HdxPickHit& hit)const;
    

    // Insert a primitive to hydra scene
    void InsertPrim(
        MayaHydraAdapter* adapter,
        const TfToken& typeId,
        const SdfPath& id);

    // Remove a primitive from hydra scene
    void RemovePrim(const SdfPath& id);

    void MarkRprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkSprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkBprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkInstancerDirty(const SdfPath& id, HdDirtyBits dirtyBits);

    // Operation that's performed on rendering a frame
    void PreFrame(const MHWRender::MDrawContext& drawContext);
    void PostFrame();

    void SetParams(const MayaHydraParams& params);
    const MayaHydraParams& GetParams() const { return _params; }

    VtValue GetShadingStyle(const SdfPath& id);

    // Adapter operations
    void RemoveAdapter(const SdfPath& id);
    void RecreateAdapter(const SdfPath& id, const MObject& obj);
    void RecreateAdapterOnIdle(const SdfPath& id, const MObject& obj);
    void RebuildAdapterOnIdle(const SdfPath& id, uint32_t flags);

    // Update viewport info to camera
    SdfPath SetCameraViewport(const MDagPath& camPath, const GfVec4d& viewport);

    // Enable or disable lighting
    void SetLightsEnabled(const bool enabled) { _lightsEnabled = enabled; }
    bool GetLightsEnabled() const { return _lightsEnabled; }

    // Enable or disable default lighting
    void SetDefaultLightEnabled(const bool enabled);
    bool GetDefaultLightEnabled() const { return _useMayaDefaultLight; }
    void SetDefaultLight(const GlfSimpleLight& light);
    const GlfSimpleLight& GetDefaultLight() const { return _mayaDefaultLight; }

    // Dag Node operations
    void InsertDag(const MDagPath& dag);
    void OnDagNodeAdded(const MObject& obj);
    void OnDagNodeRemoved(const MObject& obj);
    void AddNewInstance(const MDagPath& dag);
    void UpdateLightVisibility(const MDagPath& dag);

    void MaterialTagChanged(const SdfPath& id);
    SdfPath GetMaterialId(const SdfPath& id);
    VtValue GetMaterialResource(const SdfPath& id);

    GfInterval GetCurrentTimeSamplingInterval() const;

    HdChangeTracker& GetChangeTracker();

    HdRenderIndex& GetRenderIndex() { return *_renderIndex; }

    SdfPath GetDelegateID(TfToken name);

    HdMeshTopology GetMeshTopology(const SdfPath& id);

    SdfPath GetPrimPath(const MDagPath& dg, bool isSprim) const;

    SdfPath GetLightedPrimsRootPath() const;

    SdfPath GetRprimPath() const { return _rprimPath; }

    bool IsHdSt() const { return _isHdSt; }

    bool GetPlaybackRunning() const;

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const override;

    //Sdfpath of the maya default material
    SdfPath GetDefaultMaterialPath() const{return _mayaDefaultMaterialPath;}

    //Is the exclusion list of materials that should be skipped when using the default material
    SdfPathVector GetDefaultMaterialExclusionPaths()const{ return {_mayaFacesSelectionMaterialPath};}

    // Common function to return templated sample types
    template <typename T, typename Getter>
    size_t SampleValues(size_t maxSampleCount, float* times, T* samples, Getter getValue)
    {
        if (ARCH_UNLIKELY(maxSampleCount == 0)) {
            return 0;
        }
        // Fast path 1 sample at current-frame
        if (maxSampleCount == 1
            || (!GetParams().motionSamplesEnabled() && GetParams().motionSampleStart == 0)) {
            times[0] = 0.0f;
            samples[0] = getValue();
            return 1;
        }

        const GfInterval shutter = GetCurrentTimeSamplingInterval();
        // Shutter for [-1, 1] (size 2) should have a step of 2 for 2 samples, and 1 for 3 samples
        // For sample size of 1 tStep is unused and we match USD and to provide t=shutterOpen
        // sample.
        const double tStep = maxSampleCount > 1 ? (shutter.GetSize() / (maxSampleCount - 1)) : 0;
        const MTime  mayaTime = MAnimControl::currentTime();
        size_t       nSamples = 0;
        double       relTime = shutter.GetMin();

        for (size_t i = 0; i < maxSampleCount; ++i) {
            T sample;
            {
                MDGContextGuard guard(mayaTime + relTime);
                sample = getValue();
            }
            // We compare the sample to the previous in order to reduce sample count on output.
            // Goal is to reduce the amount of samples/keyframes the Hydra delegate has to absorb.
            if (!nSamples || sample != samples[nSamples - 1]) {
                samples[nSamples] = std::move(sample);
                times[nSamples] = relTime;
                ++nSamples;
            }
            relTime += tStep;
        }
        return nSamples;
    }
   
    /// Is using an environment variable to tell if we should pass normals to Hydra when using the render item and mesh adapters
    static bool passNormalsToHydra();

    ///Create the default material from the "standardsurface1" maya material or create a fallback material if it cannot be found
    void CreateMayaDefaultMaterialData();
    
private:
    MayaHydraSceneIndex(
        MayaHydraInitData& initData,
        bool lightEnabled);

    template <typename AdapterPtr, typename Map>
    AdapterPtr _CreateAdapter(
        const MDagPath& dag,
        const std::function<AdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>& adapterCreator,
        Map& adapterMap,
        bool isSprim = false);
    MayaHydraLightAdapterPtr CreateLightAdapter(const MDagPath& dagPath);
    MayaHydraCameraAdapterPtr CreateCameraAdapter(const MDagPath& dagPath);
    MayaHydraShapeAdapterPtr CreateShapeAdapter(const MDagPath& dagPath);

    // Utilites
    bool _GetRenderItem(int fastId, MayaHydraRenderItemAdapterPtr& adapter);
    void _AddPrimAncestors(const SdfPath& path);
    void _AddRenderItem(const MayaHydraRenderItemAdapterPtr& ria);
    void _RemoveRenderItem(const MayaHydraRenderItemAdapterPtr& ria);
    bool _GetRenderItemMaterial(const MRenderItem& ri, SdfPath& material, MObject& shadingEngineNode);
    SdfPath _GetRenderItemPrimPath(const MRenderItem& ri);
    SdfPath GetMaterialPath(const MObject& obj);
    bool _CreateMaterial(const SdfPath& id, const MObject& obj);
    
    using LightDagPathMap = std::unordered_map<std::string, MDagPath>;
    LightDagPathMap _GetGlobalLightPaths() const;
    
    static VtValue  _CreateDefaultMaterialFallback();
    static VtValue  _CreateMayaFacesSelectionMaterial();

    using DirtyBitsToLocatorsFunc = std::function<void(TfToken const&, const HdDirtyBits, HdDataSourceLocatorSet*)>;
    void _MarkPrimDirty(
        const SdfPath&          id,
        HdDirtyBits             dirtyBits,
        DirtyBitsToLocatorsFunc dirtyBitsToLocatorsFunc);
private:
    // ------------------------------------------------------------------------
    // HdSceneIndexBase implementations
    // TODO: Reuse the implementations from HdRetainedSceneIndex with usd 23.05+
    struct _PrimEntry
    {
        HdSceneIndexPrim prim;
    };
    using _PrimEntryTable = SdfPathTable<_PrimEntry>;
    _PrimEntryTable _entries;

    SdfPath _ID;
    MayaHydraParams _params;

    HdRenderIndex* _renderIndex = nullptr;

    // Adapters
    AdapterMap<MayaHydraLightAdapterPtr> _lightAdapters;
    AdapterMap<MayaHydraCameraAdapterPtr> _cameraAdapters;
    AdapterMap<MayaHydraShapeAdapterPtr> _shapeAdapters;
    AdapterMap<MayaHydraRenderItemAdapterPtr> _renderItemsAdapters;
    std::unordered_map<int, MayaHydraRenderItemAdapterPtr> _renderItemsAdaptersFast;
    AdapterMap<MayaHydraMaterialAdapterPtr>    _materialAdapters;
    std::vector<MCallbackId>                   _callbacks;
    std::vector<std::tuple<SdfPath, MObject>>  _adaptersToRecreate;
    std::vector<std::tuple<SdfPath, uint32_t>> _adaptersToRebuild;

    std::vector<MObject> _addedNodes;
    using LightAdapterCreator
        = std::function<MayaHydraLightAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    std::vector<std::pair<MObject, LightAdapterCreator>> _lightsToAdd;
    std::vector<SdfPath> _materialTagsChanged;

    bool _useDefaultMaterial = false;
    static SdfPath _fallbackMaterial;
    /// _mayaDefaultMaterialPath is common to all scene indexes
    static SdfPath _mayaDefaultMaterialPath;
    /// _mayaDefaultMaterial is a Hydra material used to override all materials from the scene when
    /// _useDefaultMaterial is true
    static VtValue _mayaDefaultMaterialFallback;//Used only if we cannot find the default material named standardsurface1

    /// _mayaFacesSelectionMaterialPath is a path to a Hydra material used to display the faces selection on nodes when being in components selection mode
    static SdfPath _mayaFacesSelectionMaterialPath;
    /// _mayaFacesSelectionMaterial is a Hydra material used to display the faces selection on nodes when being in components selection mode
    VtValue _mayaFacesSelectionMaterial;

    // Default light
    GlfSimpleLight _mayaDefaultLight;
    bool _useMayaDefaultLight = false;
    static SdfPath _mayaDefaultLightPath;

    bool _xRayEnabled = false;
    bool _isPlaybackRunning = false;
    bool _lightsEnabled = true;
    bool _isHdSt = false;

    SdfPath _rprimPath;
    SdfPath _sprimPath;
    SdfPath _materialPath;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRASCENEINDEX_H
