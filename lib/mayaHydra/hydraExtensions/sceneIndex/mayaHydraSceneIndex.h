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

#include "pxr/imaging/hd/dirtyBitsTranslator.h"

#include <mayaHydraLib/adapters/cameraAdapter.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/materialAdapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/sceneIndex/mayaHydraMaterialDataSource.h>

#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MDagPath.h>
#include <maya/MObject.h>
#include <ufe/ufe.h>

#include <flowViewport/selection/fvpPathMapperFwd.h>
#include <flowViewport/selection/fvpSelectionTypes.h>

#include <unordered_map>

UFE_NS_DEF { class Path; }

// Forward declaration so that qualified friend declaration is valid.
namespace MAYAHYDRA_NS_DEF { class BatchRenderer; }

PXR_NAMESPACE_OPEN_SCOPE

struct MayaHydraInitData
{
    MayaHydraInitData(
        TfToken        nameIn,
        HdRenderIndex& renderIndexIn,
        const SdfPath& delegateIDIn,
        bool           isHdStIn)
        : name(nameIn)
        , renderIndex(renderIndexIn)
        , delegateID(delegateIDIn)
        , isHdSt(isHdStIn)
    {
    }

    TfToken        name;
    HdRenderIndex& renderIndex;
    SdfPath        delegateID;
    bool           isHdSt;
};

class MayaHydraSceneIndex;
TF_DECLARE_WEAK_AND_REF_PTRS(MayaHydraSceneIndex);
/**
 * \brief MayaHydraSceneIndex is a scene index to produce the hydra scene from Maya native scene.
 */
class MAYAHYDRALIB_API MayaHydraSceneIndex : public HdRetainedSceneIndex
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
        bool interactive) {
        return TfCreateRefPtr(new MayaHydraSceneIndex(initData, interactive));
    }

    ~MayaHydraSceneIndex();

    // ------------------------------------------------------------------------
    // Maya Hydra scene producer implementations

    // Method to update render item data translation. Code in this method should pertain
    // ONLY to render items, such that if there is no render item data to be translated,
    // this method should not need to be called.
    void UpdateRenderItems(const MDataServerOperation::MViewportScene& scene);

    // Populate data from Maya
    void Populate();

    // Insert a primitive to hydra scene
    void InsertPrim(MayaHydraAdapter* adapter, const TfToken& typeId, const SdfPath& id);

    // Remove a primitive from hydra scene
    void RemovePrim(const SdfPath& id);

    void MarkRprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkSprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkBprimDirty(const SdfPath& id, HdDirtyBits dirtyBits);
    void MarkInstancerDirty(const SdfPath& id, HdDirtyBits dirtyBits);

    // The scene index does not update the Hydra scene in real-time for every Maya operation.
    // Call this method to ensure the Hydra scene is fully up-to-date with the Maya scene.
    void FlushPendingUpdates();

    void                   SetParams(const MayaHydraParams& params);
    const MayaHydraParams& GetParams() const { return _params; }

    VtValue GetShadingStyle(const SdfPath& id);

    // Adapter operations
    void RemoveAdapter(const SdfPath& id);
    void RecreateAdapter(const SdfPath& id, const MObject& obj);
    void RecreateAdapterOnIdle(const SdfPath& id, const MObject& obj);
    void RebuildAdapterOnIdle(const SdfPath& id, uint32_t flags);

    template <typename AdapterType>
    const AdapterMap<std::shared_ptr<AdapterType>>& GetAdapterMap() const
    {
        if constexpr (std::is_same_v<AdapterType, MayaHydraLightAdapter>) {
            return _lightAdapters;
        } else if constexpr (std::is_same_v<AdapterType, MayaHydraCameraAdapter>) {
            return _cameraAdapters;
        } else if constexpr (std::is_same_v<AdapterType, MayaHydraShapeAdapter>) {
            return _shapeAdapters;
        } else if constexpr (std::is_same_v<AdapterType, MayaHydraRenderItemAdapter>) {
            return _renderItemsAdapters;
        } else if constexpr (std::is_same_v<AdapterType, MayaHydraMaterialAdapter>) {
            return _materialAdapters;
        } else {
            static_assert(!sizeof(AdapterType*), "Unsupported adapter type");
        }
    }

    template<typename AdapterType>
    AdapterType* FindAdapter(const SdfPath& id) const
    {
        auto adapterMap = GetAdapterMap<AdapterType>();
        auto* ptrToAdapterPtr = TfMapLookupPtr(adapterMap, id);
        if (ptrToAdapterPtr != nullptr) {
            auto adapterPtr = *ptrToAdapterPtr;
            return static_cast<AdapterType*>(adapterPtr.get());
        }
        return nullptr;
    }

    // Update viewport info to camera
    SdfPath SetCameraViewport(const MDagPath& camPath, const GfVec4d& viewport);

    // Enable or disable shadows
    void SetShadowsEnabled(const bool enabled) { _shadowsEnabled = enabled; }

    // Update ShadowCollection for lights
    void UpdateLightsShadowCollection();

    // Dag Node operations
    void InsertDag(const MDagPath& dag);
    void OnDagNodeAdded(const MObject& obj);
    void OnDagNodeRemoved(const MObject& obj);
    void AddNewInstance(const MDagPath& dag);
    void UpdateLightVisibility(const MDagPath& dag);

    bool    CreateMaterial(const SdfPath& id, const MObject& obj);
    void    MaterialTagChanged(const SdfPath& id);
    SdfPath GetMaterialId(const SdfPath& id);
    VtValue GetMaterialResource(const SdfPath& id);

    GfInterval GetCurrentTimeSamplingInterval() const;

    HdRenderIndex& GetRenderIndex() { return _renderIndex; }

    SdfPath GetDelegateID(TfToken name);

    HdMeshTopology GetMeshTopology(const SdfPath& id);

    HdBasisCurvesTopology GetBasisCurvesTopology(const SdfPath& id);

    SdfPath GetPrimPath(const MDagPath& dg, bool isSprim) const;

    SdfPath GetRprimPath() const { return _rprimPath; }

    bool IsHdSt() const { return _isHdSt; }

    Fvp::PrimSelections UfePathToPrimSelections(const Ufe::Path& appPath) const;
    Fvp::PrimSelections UfePathToPrimSelectionsLit(const Ufe::Path& appPath) const;

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

    /// Is using an environment variable to tell if we should pass normals to Hydra when using the
    /// render item and mesh adapters
    static bool passNormalsToHydra();

    /// Is using an environment variable to tell if we should use the mesh adapter instead of the
    /// render item adapter for Maya meshes or when we are using batch production rendering it should be always on
    bool useMeshAdapter();
    
    using LightDagPathMap = std::unordered_map<std::string, MDagPath>;
    LightDagPathMap GetGlobalLightPaths() const;

    /// Get all paths of all lighted prims
    void GetLightedPrimPaths(SdfPathVector& lightedPrimPaths);

    GfBBox3d GetBoundingBox() const;

private:

    MayaHydraSceneIndex(
        MayaHydraInitData& initData,
        bool interactive);

    template <typename AdapterPtr, typename Map>
    AdapterPtr _CreateAdapter(
        const MDagPath&                                                         dag,
        const std::function<AdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>& adapterCreator,
        Map&                                                                    adapterMap,
        bool                                                                    isSprim = false);
    MayaHydraLightAdapterPtr  CreateLightAdapter(const MDagPath& dagPath);
    MayaHydraCameraAdapterPtr CreateCameraAdapter(const MDagPath& dagPath);
    MayaHydraShapeAdapterPtr  CreateShapeAdapter(const MDagPath& dagPath);

    // Utilites
    bool _GetRenderItem(int fastId, MayaHydraRenderItemAdapterPtr& adapter);
    void _AddPrimAncestors(const SdfPath& path);
    void _RemoveEmptyAncestors(const SdfPath& path);
    void _AddRenderItem(const MayaHydraRenderItemAdapterPtr& ria);
    void _RemoveRenderItem(const MayaHydraRenderItemAdapterPtr& ria);
    bool
    _GetRenderItemMaterial(const MRenderItem& ri, SdfPath& material, MObject& shadingEngineNode);
    SdfPath _GetRenderItemPrimPath(const MRenderItem& ri);
    SdfPath GetMaterialPath(const MObject& obj);
    using DirtyBitsToLocatorsFunc
        = std::function<void(TfToken const&, const HdDirtyBits, HdDataSourceLocatorSet*)>;
    void _MarkPrimDirty(
        const SdfPath&          id,
        HdDirtyBits             dirtyBits,
        DirtyBitsToLocatorsFunc dirtyBitsToLocatorsFunc);

#ifdef CODE_COVERAGE_WORKAROUND
    friend class MtohRenderOverride;
    friend class MAYAHYDRA_NS_DEF::BatchRenderer;
#endif
    void _Destroy();

private:
    SdfPath         _ID;
    MayaHydraParams _params;

    HdRenderIndex& _renderIndex;

    // Adapters
    AdapterMap<MayaHydraLightAdapterPtr>                   _lightAdapters;
    AdapterMap<MayaHydraCameraAdapterPtr>                  _cameraAdapters;
    AdapterMap<MayaHydraShapeAdapterPtr>                   _shapeAdapters;
    AdapterMap<MayaHydraRenderItemAdapterPtr>              _renderItemsAdapters;
    std::unordered_map<int, MayaHydraRenderItemAdapterPtr> _renderItemsAdaptersFast;
    AdapterMap<MayaHydraMaterialAdapterPtr>                _materialAdapters;
    std::vector<MCallbackId>                               _callbacks;
    std::vector<std::tuple<SdfPath, MObject>>              _adaptersToRecreate;
    std::vector<std::tuple<SdfPath, uint32_t>>             _adaptersToRebuild;

    std::vector<MObject> _addedNodes;
    using LightAdapterCreator
        = std::function<MayaHydraLightAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    std::vector<std::pair<MObject, LightAdapterCreator>> _lightsToAdd;
    using CameraAdapterCreator
        = std::function<MayaHydraCameraAdapterPtr(MayaHydraSceneIndex*, const MDagPath&)>;
    std::vector<std::pair<MObject, CameraAdapterCreator>> _camerasToAdd;
    std::vector<SdfPath>                                  _materialTagsChanged;

    static SdfPath _fallbackMaterial;

    bool _shadowsEnabled = true;
    bool _renderCollectionChanged = false;
    bool _isHdSt = false;

    SdfPath _rprimPath;
    SdfPath _sprimPath;
    SdfPath _materialPath;

    const Fvp::PathMapperConstPtr _mayaPathMapper {};

    bool _interactive{true};
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRASCENEINDEX_H
