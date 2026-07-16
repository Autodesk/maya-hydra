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
#include "materialAdapter.h"

#include <mayaHydraLib/adapters/mhDirtyNotifier.h>

#include <mayaHydraLib/adapters/adapterRegistry.h>
#include <mayaHydraLib/adapters/materialNetworkConverter.h>
#include <mayaHydraLib/adapters/mayaAttrs.h>
#include <mayaHydraLib/adapters/tokens.h>
#include <mayaHydraLib/sceneIndex/mayaHydraSceneIndex.h>

#include <pxr/base/tf/stl.h>
#include <pxr/base/tf/token.h>
#include <pxr/base/tf/type.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/changeTracker.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/types.h>
#include <pxr/usd/usd/prim.h>
#include <pxr/usd/usd/stage.h>
#ifdef WANT_MATERIALX_BUILD
#include <pxr/imaging/hdMtlx/hdMtlx.h>
#include <pxr/usd/usdMtlx/reader.h>
#endif
#include <pxr/usd/usdShade/material.h>
#include <pxr/usdImaging/usdImaging/materialParamUtils.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

#include <maya/MNodeMessage.h>
#include <maya/MPlug.h>
#include <maya/MPlugArray.h>

#include <set>
#include <string>
#include <vector>

#ifdef WANT_MATERIALX_BUILD
#include <MaterialXCore/Document.h>
#include <MaterialXFormat/File.h>
#include <MaterialXFormat/XmlIo.h>
#endif

PXR_NAMESPACE_OPEN_SCOPE

namespace {

TF_DEFINE_PRIVATE_TOKENS(
    _tokens,
    (mtlx)
    ((mtlxSurface, "mtlx:surface"))
    (surface)
);

const VtValue       _emptyValue;
const TfToken       _emptyToken;
const TfTokenVector _stSamplerCoords = { TfToken("st") };

#ifdef WANT_MATERIALX_BUILD
// Collect the node-definition identifiers referenced by 'doc' (including inside
// nodegraphs) that cannot currently be resolved from the document itself.
void _CollectUnresolvedNodeDefs(const MaterialX::DocumentPtr& doc, std::set<std::string>& out)
{
    std::vector<MaterialX::NodePtr> nodes = doc->getNodes();
    for (const auto& nodeGraph : doc->getNodeGraphs()) {
        const auto graphNodes = nodeGraph->getNodes();
        nodes.insert(nodes.end(), graphNodes.begin(), graphNodes.end());
    }
    for (const auto& node : nodes) {
        if (node && !node->getNodeDef()) {
            const std::string def = node->getNodeDefString();
            if (!def.empty()) {
                out.insert(def);
            }
        }
    }
}
#endif // WANT_MATERIALX_BUILD

} // namespace

/* MayaHydraMaterialAdapter is used to handle the translation from a Maya material to hydra.
    If you are looking for how we translate the Maya shaders to hydra and how we do the parameters
   mapping, please see MayaHydraMaterialNetworkConverter::initialize().
*/

MayaHydraMaterialAdapter::MayaHydraMaterialAdapter(
    const SdfPath&        id,
    MayaHydraSceneIndex* mayaHydraSceneIndex,
    const MObject&        node)
    : MayaHydraAdapter(node, id, mayaHydraSceneIndex)
{
}

bool MayaHydraMaterialAdapter::IsSupported() const
{
    return GetMayaHydraSceneIndex()->IsSprimTypeSupported(HdPrimTypeTokens->material);
}

bool MayaHydraMaterialAdapter::HasType(const TfToken& typeId) const
{
    return typeId == HdPrimTypeTokens->material;
}

void MayaHydraMaterialAdapter::RemovePrim()
{
    if (!_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->RemovePrim(GetID());
    _isPopulated = false;
}

void MayaHydraMaterialAdapter::Populate()
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_GET)
        .Msg("MayaHydraMaterialAdapter::Populate() - %s\n", GetID().GetText());
    if (_isPopulated) {
        return;
    }
    GetMayaHydraSceneIndex()->InsertPrim(this, HdPrimTypeTokens->material, GetID());
    _isPopulated = true;
}

void MayaHydraMaterialAdapter::EnableXRayShadingMode(bool enable)
{
    _enableXRayShadingMode = enable;
    if (_isPopulated) {
        MayaHydra::DirtyNotifier notifier(this);
        notifier.dirtyMaterial();
        notifier.flush();
    }
}

VtValue MayaHydraMaterialAdapter::GetMaterialResource()
{
    TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
        .Msg("MayaHydraMaterialAdapter::GetMaterialResource()\n");
    return GetPreviewMaterialResource(GetID());
}

VtValue MayaHydraMaterialAdapter::GetPreviewMaterialResource(const SdfPath& materialID)
{
    HdMaterialNetworkMap map;
    HdMaterialNetwork    network;
    HdMaterialNode       node;
    node.path = materialID;
    node.identifier
        = UsdImagingTokens->UsdPreviewSurface; // We translate to a USD preview surface material
    map.terminals.push_back(node.path);
    for (const auto& it : MayaHydraMaterialNetworkConverter::GetPreviewShaderParams()) {
        node.parameters.emplace(it.name, it.fallbackValue);
    }
    network.nodes.push_back(node);
    map.map.emplace(HdMaterialTerminalTokens->surface, network);
    return VtValue(map);
}

/**
 * \brief MayaHydraShadingEngineAdapter is used to handle the translation from a Maya shading engine
 * to hydra.
 */
class MayaHydraShadingEngineAdapter : public MayaHydraMaterialAdapter
{
public:
    typedef MayaHydraMaterialNetworkConverter::PathToMobjMap PathToMobjMap;

    MayaHydraShadingEngineAdapter(
        const SdfPath&        id,
        MayaHydraSceneIndex*  mayaHydraSceneIndex,
        const MObject&        obj)
        : MayaHydraMaterialAdapter(id, mayaHydraSceneIndex, obj)
        , _surfaceShaderCallback(0)
    {
        _CacheNodeAndTypes();
    }

    ~MayaHydraShadingEngineAdapter() override
    {
        if (_surfaceShaderCallback != 0) {
            MNodeMessage::removeCallback(_surfaceShaderCallback);
        }
    }

    void CreateCallbacks() override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_CALLBACKS)
            .Msg("Creating shading engine adapter callbacks for prim (%s).\n", GetID().GetText());

        MStatus status;
        auto    obj = GetNode();
        // Use attribute-changed only (not node-dirty) to avoid redundant material+primvars dirtying
        // when only custom attributes change.
        auto id = MNodeMessage::addAttributeChangedCallback(obj, _AttributeChangedCallback, this, &status);
        if (status) {
            AddCallback(id);
        }
        _CreateSurfaceMaterialCallback();
        MayaHydraAdapter::CreateCallbacks();
    }

    void Populate() override
    {
        MayaHydraMaterialAdapter::Populate();
#ifdef MAYAHYDRALIB_OIT_ENABLED
        _isTranslucent = IsTranslucent();
#endif
    }

private:
    static void _AttributeChangedCallback(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData)
    {
        auto* adapter = reinterpret_cast<MayaHydraShadingEngineAdapter*>(clientData);
        if (MayaHydraAdapter::ShouldSkipHydraUpdates(adapter->GetMayaHydraSceneIndex())) {
            return;
        }
        if (MayaHydraAdapter::IsExtensionOrDynamicAttribute(plug)) {
            adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
            return;
        }
        adapter->_CreateSurfaceMaterialCallback();
        {
            MayaHydra::DirtyNotifier notifier(adapter);
            notifier.dirtyMaterial();
            notifier.flush();
        }
    }
    static void _ShaderAttributeChangedCallback(
        MNodeMessage::AttributeMessage msg,
        MPlug&                         plug,
        MPlug&                         otherPlug,
        void*                          clientData)
    {
        TF_UNUSED(msg);
        TF_UNUSED(otherPlug);
        auto* adapter = reinterpret_cast<MayaHydraShadingEngineAdapter*>(clientData);
        if (MayaHydraAdapter::ShouldSkipHydraUpdates(adapter->GetMayaHydraSceneIndex())) {
            return;
        }
        if (MayaHydraAdapter::IsExtensionOrDynamicAttribute(plug)) {
            adapter->MaybeMarkPrimvarDirtyForAttributeChange(plug);
            return;
        }
        {
            MayaHydra::DirtyNotifier notifier(adapter);
            notifier.dirtyMaterial();
            notifier.flush();
        }
        if (adapter->GetMayaHydraSceneIndex()->IsHdSt()) {
            adapter->GetMayaHydraSceneIndex()->MaterialTagChanged(adapter->GetID());
        }
    }

    void _CacheNodeAndTypes()
    {
        _surfaceShader = MObject::kNullObj;
        _surfaceShaderType = _emptyToken;
        MStatus           status;
        MFnDependencyNode node(GetNode(), &status);
        if (ARCH_UNLIKELY(!status)) {
            return;
        }

        auto       p = node.findPlug(MayaAttrs::shadingEngine::surfaceShader, true);
        MPlugArray conns;
        p.connectedTo(conns, true, false);

        // LookdevX / MaterialX materials connect to the separate
        // "materialXSurfaceShader" attribute rather than "surfaceShader".
        if (conns.length() == 0) {
            auto pMtlx = node.findPlug("materialXSurfaceShader", true);
            if (!pMtlx.isNull()) {
                pMtlx.connectedTo(conns, true, false);
            }
        }

        if (conns.length() > 0) {
            _surfaceShader = conns[0].node();
            MFnDependencyNode surfaceNode(_surfaceShader, &status);
            if (ARCH_UNLIKELY(!status)) {
                return;
            }
            _surfaceShaderType = TfToken(surfaceNode.typeName().asChar());
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg(
                    "Found surfaceShader %s[%s]\n",
                    surfaceNode.name().asChar(),
                    _surfaceShaderType.GetText());
        }
    }

    void _CreateSurfaceMaterialCallback()
    {
        _CacheNodeAndTypes();
        if (_surfaceShaderCallback != 0) {
            MNodeMessage::removeCallback(_surfaceShaderCallback);
            _surfaceShaderCallback = 0;
        }

        if (_surfaceShader != MObject::kNullObj) {
            MStatus status;
            _surfaceShaderCallback = MNodeMessage::addAttributeChangedCallback(
                _surfaceShader, _ShaderAttributeChangedCallback, this, &status);
            if (!status) {
                _surfaceShaderCallback = 0;
            }
        }
    }

    bool PopulateMaterialXNetworkMap(HdMaterialNetworkMap& networkMap)
    {
#ifdef WANT_MATERIALX_BUILD
        // Get the dependency node
        MStatus status;
        MFnDependencyNode node(_surfaceShader, &status);
        if (!status) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: Failed to get surface shader node\n");
            return false;
        }

        // Fetch the "renderDocument" attribute from the node
        static const MString renderDocumentStr("renderDocument");
        auto mtlxDocPlug = node.findPlug(renderDocumentStr, true);
        if (mtlxDocPlug.isNull()) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: renderDocument plug not found on node %s\n",
                     node.name().asChar());
            return false;
        }

        // Construct a MaterialX document
        MString mtlxDocStr = mtlxDocPlug.asString();

        if (0 == mtlxDocStr.length()) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: renderDocument is empty, attempting to force evaluation on node %s\n",
                     node.name().asChar());

            // In batch render mode, LookdevX may not have evaluated the node yet.
            // Pull on the actual source plug that drives the shading engine's
            // "materialXSurfaceShader" attribute — this is the definitive output of the
            // LookdevX node and evaluating it triggers its compute function regardless of
            // what the output attribute is named (it is not necessarily "outColor").
            {
                MStatus seStatus;
                MFnDependencyNode seNode(GetNode(), &seStatus); // GetNode() = shading engine
                if (seStatus) {
                    MPlug sePlug = seNode.findPlug("materialXSurfaceShader", true);
                    if (!sePlug.isNull()) {
                        MPlug srcPlug = sePlug.source(); // output plug on the LookdevX node
                        if (!srcPlug.isNull()) {
                            try {
                                MObject val;
                                srcPlug.getValue(val);
                                TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                                    .Msg("PopulateMaterialXNetworkMap: Triggered evaluation via source plug %s, retrying renderDocument\n",
                                         srcPlug.name().asChar());
                                mtlxDocStr = mtlxDocPlug.asString();
                            } catch (...) {
                                TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                                    .Msg("PopulateMaterialXNetworkMap: Source plug evaluation failed\n");
                            }
                        }
                    }
                }
            }

            if (0 == mtlxDocStr.length()) {
                TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                    .Msg("PopulateMaterialXNetworkMap: renderDocument still empty after evaluation attempt\n");
                return false;
            }
        }

        TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
            .Msg("PopulateMaterialXNetworkMap: renderDocument length = %d on node %s\n",
                 (int)mtlxDocStr.length(), node.name().asChar());
        auto mtlxDoc = MaterialX::createDocument();

        // Pass explicit search paths so that any <xi:include> references in the
        // renderDocument (e.g. custom LookdevX / shader libraries) are resolved.
        // Use USD's HdMtlxSearchPaths() - the same paths the rest of Hydra/USD use -
        // and intentionally do NOT inject extra libraries or copy custom node
        // definitions into the document. Doing so changes the network UsdMtlxRead
        // produces (wrong material terminal / corrupted graph) and breaks the material
        // in both the viewport and batch. If a custom definition cannot be resolved
        // with these paths, the warning below reports it instead.
        const MaterialX::FileSearchPath searchPaths = HdMtlxSearchPaths();
        try {
            MaterialX::readFromXmlString(mtlxDoc, mtlxDocStr.asChar(), searchPaths);
        } catch (const std::exception& e) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: Failed to parse MaterialX XML: %s\n", e.what());
            return false;
        }

        // Warn about node definitions that still cannot be resolved.
        // UsdMtlxRead drops unresolved nodes together with the branches feeding them,
        // which silently diverges the viewport from a batch/USD render, so surface the
        // missing definitions and the search paths used to make the failure actionable.
        {
            std::set<std::string> unresolvedDefs;
            _CollectUnresolvedNodeDefs(mtlxDoc, unresolvedDefs);
            if (!unresolvedDefs.empty()) {
                std::string defList;
                for (const std::string& def : unresolvedDefs) {
                    defList += (defList.empty() ? "" : ", ") + def;
                }
                TF_WARN(
                    "MaterialX material '%s': %zu node definition(s) could not be "
                    "resolved [%s]. These nodes and the branches feeding them will be "
                    "dropped, so the viewport may not match a batch/USD render. Check "
                    "the MaterialX library search paths (MATERIALX_SEARCH_PATH / "
                    "PXR_MTLX_STDLIB_SEARCH_PATHS). Search paths used: %s",
                    node.name().asChar(),
                    unresolvedDefs.size(),
                    defList.c_str(),
                    searchPaths.asString().c_str());
            }
        }

        // Create a Usd Stage from the MaterialX document
        auto stage = UsdStage::CreateInMemory("tmp.usda", TfNullPtr);
        UsdMtlxRead(mtlxDoc, stage);

        // Search for material group in the Usd Stage
        static const SdfPath basePath("/MaterialX/Materials");
        auto mtlxRange = stage->GetPrimAtPath(basePath).GetChildren();
        if (mtlxRange.empty()) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: No materials found at /MaterialX/Materials\n");
            return false;
        }

        // There should be only one material. Fetch it.
        UsdShadeMaterial mtlxMaterial(*mtlxRange.begin());
        if (!mtlxMaterial) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: Failed to get material from stage\n");
            return false;
        }

        // Get MaterialX output
        UsdShadeOutput mtlxOutput = mtlxMaterial.GetOutput(_tokens->mtlxSurface);
        if (!mtlxOutput) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: Failed to get mtlx:surface output\n");
            return false;
        }

        // Get MaterialX shader outputs
        UsdShadeAttributeVector mtlxShaderOutputs =
            UsdShadeUtils::GetValueProducingAttributes(mtlxOutput, /*shaderOutputsOnly*/true);
        if (mtlxShaderOutputs.empty()) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: No shader outputs found\n");
            return false;
        }

        // Finally get MaterialX shader
        UsdShadeShader mtlxShader(mtlxShaderOutputs[0].GetPrim());
        if (!mtlxShader) {
            TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
                .Msg("PopulateMaterialXNetworkMap: Failed to get shader prim\n");
            return false;
        }

        TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
            .Msg("PopulateMaterialXNetworkMap: Successfully extracted MaterialX shader at path %s\n",
                 mtlxShader.GetPrim().GetPath().GetText());

        // Convert the MaterialX shader to HdMaterialNetwork
        UsdImagingBuildHdMaterialNetworkFromTerminal(
            mtlxShader.GetPrim(), _tokens->surface, {_tokens->mtlx},
            {_tokens->mtlx}, &networkMap, UsdTimeCode());

        TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
            .Msg("PopulateMaterialXNetworkMap: Built HdMaterialNetwork with %zu terminal entries\n",
                 networkMap.map.size());

        return true;
#else
        // MaterialX support (hdMtlx / usdMtlx) is not available in this USD build,
        // so there is no MaterialX network to build. Returning false makes the
        // caller fall back to the standard Maya material network conversion.
        (void)networkMap;
        return false;
#endif // WANT_MATERIALX_BUILD
    }

    VtValue GetMaterialResource() override
    {
        TF_DEBUG(MAYAHYDRALIB_ADAPTER_MATERIALS)
            .Msg("MayaHydraShadingEngineAdapter::GetMaterialResource(): %s\n", GetID().GetText());
        
        HdMaterialNetworkMap materialXNetworkMap;
        if (PopulateMaterialXNetworkMap(materialXNetworkMap)) {
            return VtValue(materialXNetworkMap);
        }
        
        MayaHydraMaterialNetworkConverter::MayaHydraMaterialNetworkConverterInit initStruct(
            GetID(), _enableXRayShadingMode, &_materialPathToMobj);

        MayaHydraMaterialNetworkConverter converter(initStruct);
        if (!converter.GetMaterial(_surfaceShader)) {
            return GetPreviewMaterialResource(GetID());
        }

        HdMaterialNetworkMap materialNetworkMap;
        materialNetworkMap.map[HdMaterialTerminalTokens->surface] = initStruct._materialNetwork;
        if (!initStruct._materialNetwork.nodes.empty()) {
            materialNetworkMap.terminals.push_back(initStruct._materialNetwork.nodes.back().path);
        }

        // HdMaterialNetwork displacementNetwork;
        // materialNetworkMap.map[HdMaterialTerminalTokens->displacement] =
        // displacementNetwork;

        return VtValue(materialNetworkMap);
    };

#ifdef MAYAHYDRALIB_OIT_ENABLED
    bool UpdateMaterialTag() override
    {
        if (IsTranslucent() != _isTranslucent) {
            _isTranslucent = !_isTranslucent;
            return true;
        }
        return false;
    }

    bool IsTranslucent()
    {
        if (_surfaceShaderType == MayaHydraAdapterTokens->usdPreviewSurface
            || _surfaceShaderType == MayaHydraAdapterTokens->pxrUsdPreviewSurface) {
            MFnDependencyNode node(_surfaceShader);
            const auto        plug = node.findPlug(MayaHydraAdapterTokens->opacity.GetText(), true);
            if (!plug.isNull() && (plug.asFloat() < 1.0f || plug.isConnected())) {
                return true;
            }
        }
        return false;
    }

#endif // MAYAHYDRALIB_OIT_ENABLED

    PathToMobjMap _materialPathToMobj;

    MObject _surfaceShader;
    TfToken _surfaceShaderType;
    // So they live long enough

    MCallbackId _surfaceShaderCallback;
#ifdef MAYAHYDRALIB_OIT_ENABLED
    bool _isTranslucent = false;
#endif
};

TF_REGISTRY_FUNCTION(TfType)
{
    TfType::Define<MayaHydraMaterialAdapter, TfType::Bases<MayaHydraAdapter>>();
    TfType::Define<MayaHydraShadingEngineAdapter, TfType::Bases<MayaHydraMaterialAdapter>>();
}

TF_REGISTRY_FUNCTION_WITH_TAG(MayaHydraAdapterRegistry, shadingEngine)
{
    MayaHydraAdapterRegistry::RegisterMaterialAdapter(
        TfToken("shadingEngine"),
        [](const SdfPath&        id,
            MayaHydraSceneIndex* mayaHydraSceneIndex,
           const MObject&        obj) -> MayaHydraMaterialAdapterPtr {
            return MayaHydraMaterialAdapterPtr(
                new MayaHydraShadingEngineAdapter(id, mayaHydraSceneIndex, obj));
        });
}

PXR_NAMESPACE_CLOSE_SCOPE
