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

#include "mayaViewportSceneIndex.h"

#include "mayaHydraSceneIndexUtils.h"

#include <mayaHydraLib/mayaUtils.h>
#include <mayaHydraLib/pick/mhPickHandler.h>
#include <mayaHydraLib/pick/mhPickHandlerRegistry.h>
#include <mayaHydraLib/pick/mhPickHit.h>

#include <flowViewport/colorPreferences/fvpColorPreferencesTokens.h>

#include <maya/MGlobal.h>
#include <maya/MItSelectionList.h>
#include <maya/MMaterial.h>

#include <pxr/imaging/hd/containerDataSourceEditor.h>
#include <pxr/imaging/hd/dataSourceTypeDefs.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

using namespace MayaHydra;

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,

    ((MayaDefaultMaterial, "__maya_default_material__"))
    ((MayaFacesSelectionMaterial, "__maya_faces_selection_material__"))
    ((MayaDefaultLight, "__maya_default_light__"))
    ((MayaFacesSelectionPrimPrefix, "PolyActiveFaces")) // When we have a render item which is a selection of faces, it always has this name in Maya.
    (diffuseColor)
    (emissiveColor)
    (opacity)
);

namespace {

// Pick handler for Maya data. As the Maya pick handler and the Maya
// scene index are circularly dependent (the Maya pick handler calls
// MayaViewportSceneIndex::AddPickHitToSelectionList() in the Maya scene index
// interface, and the Maya scene index builds the Maya pick handler), they are
// both defined here in the same implementation file.
class MayaPickHandler : public MayaHydra::PickHandler
{
    MayaViewportSceneIndex& _mayaViewportSceneIndex;

public:
    MayaPickHandler(MayaViewportSceneIndex& mayaViewportSceneIndex)
        : _mayaViewportSceneIndex(mayaViewportSceneIndex)
    {
    }

    bool handlePickHit(const Input& pickInput, Output& pickOutput) const override
    {
        // Maya does not create Hydra instances, so if the pick hit instancer
        // ID isn't empty, it's not a Maya pick hit.
        if (!pickInput.pickHit.hdxPickHit.instancerId.IsEmpty()) {
            return false;
        }

        return _mayaViewportSceneIndex.AddPickHitToSelectionList(
            pickInput.pickHit,
            pickInput.pickInfo,
            pickOutput.mayaSelection,
            pickOutput.mayaWorldSpaceHitPts);
    }

    bool inSingleNodeComponentsPick(const MayaHydra::PickHit& hit) const override
    {
        return _mayaViewportSceneIndex.IsPickedNodeInComponentsPickingMode(hit);
    }
};

HdMaterialNetworkMap _CreateMayaFacesSelectionMaterial(const SdfPath& materialPath)
{
    const GfVec4f faceSelectioncolor
        = getPreferencesColor(FvpColorPreferencesTokens->faceSelection);
    constexpr float      ogsMatchParamMult = 0.3f;
    HdMaterialNetworkMap networkMap;
    HdMaterialNetwork    network;
    HdMaterialNode       node;
    node.identifier = UsdImagingTokens->UsdPreviewSurface;
    node.path = materialPath;

    // Diffuse
    node.parameters.insert(
        { _tokens->diffuseColor,
          VtValue(
              GfVec3f(faceSelectioncolor[0], faceSelectioncolor[1], faceSelectioncolor[2])
              * ogsMatchParamMult) });

    // Emissive (component selection highlighting material should be independent of scene
    // lighting)
    node.parameters.insert(
        { _tokens->emissiveColor,
          VtValue(
              GfVec3f(faceSelectioncolor[0], faceSelectioncolor[1], faceSelectioncolor[2])
              * ogsMatchParamMult) });

    node.parameters.insert({ _tokens->opacity, VtValue(ogsMatchParamMult) });
    network.nodes.push_back(std::move(node));
    networkMap.map.insert({ HdMaterialTerminalTokens->surface, std::move(network) });
    networkMap.terminals.push_back(materialPath);
    return networkMap;
}

bool _AreLightParamsDifferent(const GlfSimpleLight& light1, const GlfSimpleLight& light2)
{
    // We only update 3 parameters in the default light : position, diffuse and specular. We don't
    // use the primitive's transform.
    return (light1.GetPosition() != light2.GetPosition()) // Position (in which we actually store a direction, updated when rotating the view for example)
        || (light1.GetDiffuse() != light2.GetDiffuse())
        || (light1.GetSpecular() != light2.GetSpecular());
}

} // namespace

namespace MAYAHYDRA_NS_DEF {

MayaViewportSceneIndex::MayaViewportSceneIndex(HdSceneIndexBaseRefPtr const& inputSceneIndex, MayaHydraSceneIndexRefPtr const& mayaDataSceneIndex)
    : HdFilteringSceneIndexBase()
    , HdEncapsulatingSceneIndexBase()
    , _observer(this)
    , _inputSceneIndex(inputSceneIndex)
    , _viewportDataSceneIndex(HdRetainedSceneIndex::New())
    , _mergingSceneIndex(HdMergingSceneIndex::New())
    , _mayaDataSceneIndex(mayaDataSceneIndex)
{
    // Create Maya faces selection material
    _mayaFacesSelectionMaterialPath = SdfPath::AbsoluteRootPath().AppendChild(_tokens->MayaFacesSelectionMaterial);
    _mayaFacesSelectionMaterial = _CreateMayaFacesSelectionMaterial(_mayaFacesSelectionMaterialPath);
    HdContainerDataSourceHandle mayaFacesSelectionMaterialDataSource;    
    if (_ConvertHdMaterialNetworkToHdDataSources(_mayaFacesSelectionMaterial, &mayaFacesSelectionMaterialDataSource)) {
        HdContainerDataSourceHandle mayaFacesSelectionContainerDataSource = HdRetainedContainerDataSource::New(HdMaterialSchemaTokens->material, mayaFacesSelectionMaterialDataSource);
        _viewportDataSceneIndex->AddPrims({ { _mayaFacesSelectionMaterialPath, HdPrimTypeTokens->material, mayaFacesSelectionContainerDataSource } });
    }

    // Create the default material
    _defaultMaterialPath = SdfPath::AbsoluteRootPath().AppendChild(_tokens->MayaDefaultMaterial);
    // Get the shading group of the default material
    MObject defaultMaterialShadingGroupObj = MMaterial::defaultMaterial().shadingEngine();
    if (defaultMaterialShadingGroupObj != MObject::kNullObj) {
        _mayaDataSceneIndex->CreateMaterial(_defaultMaterialPath, defaultMaterialShadingGroupObj);
    }

    // Add our pick handler to the pick handler registry if there is none.
    auto& phr = MayaHydra::PickHandlerRegistry::Instance();
    if (phr.RegisteredHandler(mayaDataSceneIndex->GetRprimPath()) == nullptr) {
      auto pickHandler = std::make_shared<MayaPickHandler>(*this);
      TF_AXIOM(phr.Register(mayaDataSceneIndex->GetRprimPath(), pickHandler));
      _hasPickHandlerRegistered = true;
    }

    // Setup the combined scene
    _mergingSceneIndex->AddInputScene(_inputSceneIndex, SdfPath::AbsoluteRootPath());
    _mergingSceneIndex->AddInputScene(_viewportDataSceneIndex, SdfPath::AbsoluteRootPath());
    _mergingSceneIndex->AddObserver(HdSceneIndexObserverPtr(&_observer));
}

MayaViewportSceneIndex::~MayaViewportSceneIndex()
{
    // Remove our pick handler from the pick handler registry.
    if (_hasPickHandlerRegistered) {
        auto& phr = MayaHydra::PickHandlerRegistry::Instance();
        TF_AXIOM(phr.Unregister(_mayaDataSceneIndex->GetRprimPath()));
    }
}

HdSceneIndexPrim MayaViewportSceneIndex::GetPrim(const SdfPath& primPath) const
{
    HdSceneIndexPrim prim = _mergingSceneIndex->GetPrim(primPath);
    if (primPath.GetName().substr(0, _tokens->MayaFacesSelectionPrimPrefix.size()) == _tokens->MayaFacesSelectionPrimPrefix) {
        HdContainerDataSourceEditor dsEditor(prim.dataSource);
        HdDataSourceLocator materialPathLocator = HdMaterialBindingsSchema::GetDefaultLocator()
                                                                            .Append(HdMaterialBindingsSchemaTokens->allPurpose)
                                                                            .Append(HdMaterialBindingSchemaTokens->path);
        dsEditor.Set(materialPathLocator, HdRetainedTypedSampledDataSource<SdfPath>::New(_mayaFacesSelectionMaterialPath));
        prim.dataSource = dsEditor.Finish();
    }
    return prim;
}

SdfPathVector MayaViewportSceneIndex::GetChildPrimPaths(const SdfPath& primPath) const
{
    SdfPathVector childPrimPaths = _mergingSceneIndex->GetChildPrimPaths(primPath);
    return childPrimPaths;
}

std::vector<PXR_NS::HdSceneIndexBaseRefPtr> MayaViewportSceneIndex::GetInputScenes() const
{
    return {_inputSceneIndex};
}

std::vector<PXR_NS::HdSceneIndexBaseRefPtr> MayaViewportSceneIndex::GetEncapsulatedScenes() const
{
    return {_viewportDataSceneIndex};
}

void MayaViewportSceneIndex::_PrimsAdded(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::AddedPrimEntries &entries)
{
    for (const auto& entry : entries) {
        auto materialAdapter = _mayaDataSceneIndex->FindAdapter<MayaHydraMaterialAdapter>(entry.primPath);
        if (materialAdapter) {
            materialAdapter->EnableXRayShadingMode(_isXRayEnabled);
        }

        auto renderItemAdapter = _mayaDataSceneIndex->FindAdapter<MayaHydraRenderItemAdapter>(entry.primPath);
        if (renderItemAdapter) {
            renderItemAdapter->SetPlaybackState(_isPlaybackRunning);
        }
    }
    if (!entries.empty()) {
        _SendPrimsAdded(entries);
    }
}

void MayaViewportSceneIndex::_PrimsRemoved(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RemovedPrimEntries &entries)
{
    if (!entries.empty()) {
        _SendPrimsRemoved(entries);
    }
}

void MayaViewportSceneIndex::_PrimsDirtied(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::DirtiedPrimEntries &entries)
{
    if (!entries.empty()) {
        _SendPrimsDirtied(entries);
    }
}

void MayaViewportSceneIndex::_PrimsRenamed(
    const HdSceneIndexBase &sender,
    const HdSceneIndexObserver::RenamedPrimEntries &entries)
{
    if (!entries.empty()) {
        _SendPrimsRenamed(entries);
    }
}

void MayaViewportSceneIndex::Update(const MDrawContext& viewportDrawContext)
{
    const bool isXRayEnabled = (viewportDrawContext.getDisplayStyle() & MHWRender::MFrameContext::kXray);
    if (_isXRayEnabled != isXRayEnabled) {
        _isXRayEnabled = isXRayEnabled;
        auto materialAdapters = _mayaDataSceneIndex->GetAdapterMap<MayaHydraMaterialAdapter>();
        for (const auto& [id, adapter] : materialAdapters) {
            adapter->EnableXRayShadingMode(_isXRayEnabled);
        }
    }

    const bool isPlaybackRunning = MAnimControl::isPlaying();
    if (_isPlaybackRunning != isPlaybackRunning) {
        _isPlaybackRunning = isPlaybackRunning;
        // The value has changed, we are calling SetPlaybackChanged so that every render item that
        // has its visibility dependent on the playback state should dirty its Hydra visibility flag
        // so it gets recomputed.
        auto renderItemAdapters = _mayaDataSceneIndex->GetAdapterMap<MayaHydraRenderItemAdapter>();
        for (const auto& [id, adapter] : renderItemAdapters) {
            adapter->SetPlaybackState(_isPlaybackRunning);
        }
    }

    _UpdateActiveLights(viewportDrawContext);
}

void MayaViewportSceneIndex::_UpdateActiveLights(const MDrawContext& viewportDrawContext)
{
    MayaHydraSceneIndex::LightDagPathMap globalLightPaths = _mayaDataSceneIndex->GetGlobalLightPaths();
    MayaHydraSceneIndex::LightDagPathMap activeLightPaths;
    constexpr auto considerAllSceneLights = MHWRender::MDrawContext::kFilteredIgnoreLightLimit;
    MStatus        status;
    const auto     numLights = viewportDrawContext.numberOfActiveLights(considerAllSceneLights, &status);

    if ((!status || numLights == 0) && (0 == globalLightPaths.size())) {
        auto lightAdapters = _mayaDataSceneIndex->GetAdapterMap<MayaHydraLightAdapter>();
        for (const auto& [id, adapter] : lightAdapters) {
            adapter->RemovePrim(); // Turn off all lights
        }
        return;
    }

    MIntArray intVals;
    MMatrix   matrixVal;
    for (auto i = decltype(numLights) { 0 }; i < numLights; ++i) {
        auto* lightParam = viewportDrawContext.getLightParameterInformation(i, considerAllSceneLights);
        if (lightParam == nullptr) {
            continue;
        }
        const auto lightPath = lightParam->lightPath();
        if (!lightPath.isValid()) {
            continue;
        }
        if (IsUfeItemFromMayaUsd(lightPath)) {
            // If this is a UFE light created by maya-usd, it will have already added it to Hydra
            continue;
        }

        // We do a fast look up here for any new lights that may have been added
        auto found = globalLightPaths.find(lightPath.fullPathName().asChar());
        if (found != globalLightPaths.end())
            activeLightPaths.emplace(lightPath.fullPathName().asChar(), lightPath);

        if (!lightParam->getParameter(MHWRender::MLightParameterInformation::kShadowOn, intVals)
            || intVals.length() < 1 || intVals[0] != 1) {
            continue;
        }
    }

    if (_lightsManagementSceneIndex) {
        std::set<SdfPath> disabledLights;

        // Store disabled lights to pass them to the lights management scene index
        auto lightAdapters = _mayaDataSceneIndex->GetAdapterMap<MayaHydraLightAdapter>();
        for (const auto& [id, adapter] : lightAdapters) {
            auto itActiveLightPath = activeLightPaths.find(adapter->GetDagPath().fullPathName().asChar());
            if (itActiveLightPath != activeLightPaths.end()) {
                activeLightPaths.erase(itActiveLightPath);
            } else {
                // Skip dome lights as Maya numberOfActiveLights API doesn't count active dome lights
                if (adapter->LightType() != HdPrimTypeTokens->domeLight) {
                    disabledLights.insert(adapter->GetID());
                }
            }
        }

        _lightsManagementSceneIndex->SetDisabledLightsPrims(disabledLights);
    }
}

const SdfPath& MayaViewportSceneIndex::DefaultLightPath()
{
    static SdfPath _defaultLightPath = SdfPath::AbsoluteRootPath().AppendChild(_tokens->MayaDefaultLight);
    return _defaultLightPath;
}

void MayaViewportSceneIndex::SetDefaultLightEnabled(const bool enabled)
{
    if (_defaultLightEnabled != enabled) {
        _defaultLightEnabled = enabled;

        if (_defaultLightEnabled) {
            auto defaultLightDataSource = MayaHydraDefaultLightDataSource::New(
                DefaultLightPath(), HdPrimTypeTokens->simpleLight, this);
            _viewportDataSceneIndex->AddPrims({ { DefaultLightPath(), HdPrimTypeTokens->simpleLight, defaultLightDataSource } });
        } else {
            _viewportDataSceneIndex->RemovePrims({ DefaultLightPath() });
        }
    }
}

void MayaViewportSceneIndex::SetDefaultLight(const GlfSimpleLight& light)
{
    // We only update 3 parameters in the default light : position (in which we store a direction),
    // diffuse and specular
    // We never update the transform for the default light
    if (_AreLightParamsDifferent(_defaultLight, light)) {
        // Update our light
        _defaultLight.SetDiffuse(light.GetDiffuse());
        _defaultLight.SetSpecular(light.GetSpecular());
        _defaultLight.SetPosition(light.GetPosition());
        _viewportDataSceneIndex->DirtyPrims({ { DefaultLightPath(), HdLightSchema::GetDefaultLocator() } });
    }
}

bool MayaViewportSceneIndex::IsPickedNodeInComponentsPickingMode(const MayaHydra::PickHit& hit) const
{
    // Is the picked node in components selection mode? If so, it is in the hilite list
    MSelectionList hiliteList;
    MGlobal::getHiliteList(hiliteList);
    if (hiliteList.isEmpty()){
        return false;
    }
    SdfPath hitId = hit.hdxPickHit.objectId;
    if (!hitId.HasPrefix(_mayaDataSceneIndex->GetRprimPath())) {
        return false;
    }
    auto adapter = _mayaDataSceneIndex->FindAdapter<MayaHydraRenderItemAdapter>(hitId);
    if (!adapter) {
        return false;
    }

    // Prepare the selection path of the hit item, the transform path 
    // is expected if available
    const auto& itemPath = adapter->GetDagPath();

    // Is the picked node in components selection mode? If so, it is in the hilite list
    MItSelectionList selListIter(hiliteList, MFn::kMesh); // Iterate on meshes only
    for (; !selListIter.isDone(); selListIter.next()) {
        MDagPath dagPath;
        selListIter.getDagPath(dagPath);
        if (itemPath == dagPath) {
            return true;
        }
    }

    return false;
}

bool MayaViewportSceneIndex::AddPickHitToSelectionList(
    const MayaHydra::PickHit& hit,
    const MHWRender::MSelectionInfo& /* selectInfo */,
    MSelectionList& selectionList,
    MPointArray& worldSpaceHitPts)
{
    SdfPath hitId = hit.hdxPickHit.objectId;
    // Validate that hit is indeed a Maya item. Alternatively, the rprim hit could be an rprim
    // defined by a scene index such as MayaUSD.
    if (!hitId.HasPrefix(_mayaDataSceneIndex->GetRprimPath())) {
        return false;
    }
    auto adapter = _mayaDataSceneIndex->FindAdapter<MayaHydraRenderItemAdapter>(hitId);
    if (!adapter) {
        return false;
    }

    // Prepare the selection path of the hit item, the transform path 
    // is expected if available
    const auto& itemPath = adapter->GetDagPath();

    MDagPath selectPath;
    if (MS::kSuccess != MDagPath::getAPathTo(itemPath.transform(), selectPath)) {
        selectPath = itemPath;
    }
    selectionList.add(selectPath);
    worldSpaceHitPts.append(
        hit.hdxPickHit.worldSpaceHitPoint[0],
        hit.hdxPickHit.worldSpaceHitPoint[1],
        hit.hdxPickHit.worldSpaceHitPoint[2]);
    return true;
}

void MayaViewportSceneIndex::SetLightsManagementSceneIndex(const Fvp::LightsManagementSceneIndexRefPtr& lightsManagementSceneIndex)
{
    _lightsManagementSceneIndex = lightsManagementSceneIndex;
}

} // namespace FVP_NS_DEF
