//
// Copyright 2025 Autodesk, Inc. All rights reserved.
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

#ifndef MH_MAYA_VIEWPORT_SCENE_INDEX_H
#define MH_MAYA_VIEWPORT_SCENE_INDEX_H

#include "mayaHydraSceneIndex.h"
#include "pxr/imaging/hd/dirtyBitsTranslator.h"

#include <mayaHydraLib/adapters/cameraAdapter.h>
#include <mayaHydraLib/adapters/lightAdapter.h>
#include <mayaHydraLib/adapters/materialAdapter.h>
#include <mayaHydraLib/adapters/renderItemAdapter.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>
#include <mayaHydraLib/api.h>
#include <mayaHydraLib/mayaHydraParams.h>
#include <mayaHydraLib/pick/mhPickHitFwd.h>
#include <mayaHydraLib/sceneIndex/mayaHydraDefaultLightDataSource.h>
#include <mayaHydraLib/sceneIndex/mayaHydraMaterialDataSource.h>

#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/imaging/hd/driver.h>
#include <pxr/imaging/hd/engine.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>
#include <pxr/imaging/hd/renderIndex.h>
#include <pxr/imaging/hd/rendererPlugin.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/selection.h>
#include <pxr/imaging/hdx/pickTask.h>
#include <pxr/imaging/hdx/taskController.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <maya/MDagPath.h>
#include <maya/MDrawContext.h>
#include <maya/MFrameContext.h>
#include <maya/MObject.h>
#include <maya/MSelectionList.h>
#include <ufe/ufe.h>

#include <flowViewport/sceneIndex/fvpLightsManagementSceneIndex.h>
#include <flowViewport/selection/fvpPathMapperFwd.h>
#include <flowViewport/selection/fvpSelectionTypes.h>

#include <optional>
#include <unordered_map>

UFE_NS_DEF { class Path; }

namespace MAYAHYDRA_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class MayaViewportSceneIndex;
typedef PXR_NS::TfRefPtr<MayaViewportSceneIndex> MayaViewportSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MayaViewportSceneIndex> MayaViewportSceneIndexConstRefPtr;

/**
 * \brief MayaViewportSceneIndex is a scene index to handle Maya viewport functionality.
 *
 * Since some viewport features require some custom data to be present in the Hydra scene 
 * (for example to handle default material, default light, Maya faces selections, etc.),
 * this scene index creates and maintains a HdRetainedSceneIndex to store this custom data,
 * on top of the usual input scene index.
 *
 * To avoid having to handle two scene indices in GetPrim/GetChildPrimPaths/PrimsXYZed methods,
 * a utilitary HdMergingSceneIndex is created to merge the input scene index and the HdRetainedSceneIndex.
 * This merging scene index is then used as if it was the actual input scene index of MayaViewportSceneIndex,
 * like we see in other single input filtering scene indices.
 */
class MayaViewportSceneIndex : 
    public PXR_NS::HdFilteringSceneIndexBase,
    public PXR_NS::HdEncapsulatingSceneIndexBase
{
public:
    MAYAHYDRALIB_API
    static MayaViewportSceneIndexRefPtr New(PXR_NS::HdSceneIndexBaseRefPtr const& inputSceneIndex, PXR_NS::MayaHydraSceneIndexRefPtr const& mayaDataSceneIndex)
    {
        return TfCreateRefPtr(new MayaViewportSceneIndex(inputSceneIndex, mayaDataSceneIndex));
    }

    MAYAHYDRALIB_API
    ~MayaViewportSceneIndex() override;

    MAYAHYDRALIB_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    MAYAHYDRALIB_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override;

    MAYAHYDRALIB_API
    std::vector<PXR_NS::HdSceneIndexBaseRefPtr> GetInputScenes() const override;

    MAYAHYDRALIB_API
    std::vector<PXR_NS::HdSceneIndexBaseRefPtr> GetEncapsulatedScenes() const override;

    MAYAHYDRALIB_API
    void Update(const MDrawContext& viewportDrawContext);

    // Is the exclusion list of materials that should be skipped when using the default material
    MAYAHYDRALIB_API
    PXR_NS::SdfPathVector GetDefaultMaterialExclusionPaths() const
    {
        return { _mayaFacesSelectionMaterialPath };
    }

    MAYAHYDRALIB_API
    static const PXR_NS::SdfPath& DefaultLightPath();

    MAYAHYDRALIB_API
    void SetDefaultLightEnabled(const bool enabled);

    MAYAHYDRALIB_API
    bool GetDefaultLightEnabled() const
    { 
        return _defaultLightEnabled;
    }

    MAYAHYDRALIB_API
    void SetDefaultLight(const PXR_NS::GlfSimpleLight& light);

    MAYAHYDRALIB_API
    const PXR_NS::GlfSimpleLight& GetDefaultLight() const 
    { 
        return _defaultLight;
    }

    MAYAHYDRALIB_API
    PXR_NS::SdfPath GetDefaultMaterialPath() const
    { 
        return _defaultMaterialPath;
    }

    MAYAHYDRALIB_API
    void SetLightsManagementSceneIndex(const Fvp::LightsManagementSceneIndexRefPtr& lightsManagementSceneIndex); // Can be a nullptr

protected:
    MayaViewportSceneIndex(PXR_NS::HdSceneIndexBaseRefPtr const& inputSceneIndex, PXR_NS::MayaHydraSceneIndexRefPtr const& mayaDataSceneIndex);

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries);

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries);

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries);
    
    void _PrimsRenamed(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RenamedPrimEntries& entries);

    friend class _MergingSceneIndexObserver;
    class _MergingSceneIndexObserver : public PXR_NS::HdSceneIndexObserver
    {
    public:
        _MergingSceneIndexObserver(MayaViewportSceneIndex *owner) : _owner(owner) {}
        
        void PrimsAdded(const HdSceneIndexBase &sender, const AddedPrimEntries &entries) override
        {
            _owner->_PrimsAdded(sender, entries);
        }
        
        void PrimsRemoved(const HdSceneIndexBase &sender, const RemovedPrimEntries &entries) override
        {
            _owner->_PrimsRemoved(sender, entries);
        }
        
        void PrimsDirtied(const HdSceneIndexBase &sender, const DirtiedPrimEntries &entries) override
        {
            _owner->_PrimsDirtied(sender, entries);
        }
        
        void PrimsRenamed(const HdSceneIndexBase &sender, const RenamedPrimEntries &entries) override
        {
            _owner->_PrimsRenamed(sender, entries);
        }
    private:
        MayaViewportSceneIndex* _owner;
    };
    _MergingSceneIndexObserver _observer;

    friend class MayaPickHandler;
    class MayaPickHandler;
    
    // Add Hydra pick points and items to Maya's selection list
    bool AddPickHitToSelectionList(
        const MayaHydra::PickHit&        hit,
        const MHWRender::MSelectionInfo& selectInfo,
        MSelectionList&                  selectionList,
        MPointArray&                     worldSpaceHitPts);

    bool IsPickedNodeInComponentsPickingMode(const MayaHydra::PickHit& hit) const;

    void _UpdateActiveLights(const MDrawContext& viewportDrawContext);
    
    // Input/output scene indices 
    PXR_NS::HdSceneIndexBaseRefPtr _inputSceneIndex;
    PXR_NS::HdRetainedSceneIndexRefPtr _viewportDataSceneIndex;
    PXR_NS::HdMergingSceneIndexRefPtr _mergingSceneIndex;

    // Reference to the Maya data scene index. Note that we don't use this as an input/output scene index
    // of the scene index chain, but rather because it is where much of the Maya<->Hydra data translation 
    // code is contained.
    PXR_NS::MayaHydraSceneIndexRefPtr _mayaDataSceneIndex;

    // Maya faces selection material (used to display the faces
    // selection on nodes when being in components selection mode)
    PXR_NS::SdfPath _mayaFacesSelectionMaterialPath;
    PXR_NS::HdMaterialNetworkMap _mayaFacesSelectionMaterial;

    // Default light
    bool _defaultLightEnabled{false};
    PXR_NS::GlfSimpleLight _defaultLight;

    // Default material
    PXR_NS::SdfPath _defaultMaterialPath;

    // Pick handling
    bool _hasPickHandlerRegistered{false};

    // Playback
    bool _isPlaybackRunning{false};

    // X-Ray
    bool _isXRayEnabled{false};

    // Active viewport lights
    Fvp::LightsManagementSceneIndexRefPtr _lightsManagementSceneIndex;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MH_MAYA_VIEWPORT_SCENE_INDEX_H
