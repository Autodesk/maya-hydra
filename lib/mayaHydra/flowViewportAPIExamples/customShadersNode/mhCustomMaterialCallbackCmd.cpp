//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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

//maya headers
// MFnPlugin.h defines the plugin version and entry point symbols, which must only be defined once per plugin
#define MNoVersionString
#define MNoPluginEntry
#include <maya/MFnPlugin.h>
#include <maya/MArgDatabase.h>
#include <maya/MPxCommand.h>
#include <maya/MSyntax.h>

//Flow viewport headers
#include <flowViewport/API/fvpMaterialCreationInterface.h>

//Hydra headers
#include <pxr/base/gf/vec3f.h>
#include <pxr/imaging/hd/material.h>
#include <pxr/imaging/hd/tokens.h>
#include <pxr/usdImaging/usdImaging/tokens.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

TF_DEFINE_PRIVATE_TOKENS(_tokens,
    (FVP_CustomBasicLightingShader) //Name of the shader loaded by Hydra
    (FVP_CustomColor) //Parameter of our custom color GLSLFX shader
    (displayColorPrimVarReaderNode)//Name of the prim var reader node
    (varname)
    (result)
    (fallback)
);

const char* const kUnregisterFlag     = "-u";
const char* const kUnregisterFlagLong = "-unregister";

//Material creation callback registered in the flow viewport MaterialCreationInterface.
//MayaHydra calls it for Maya GLSLShader materials, which are replaced by the FVP_CustomBasicLightingShader glslfx shader,
//their parameters are not translated.
VtValue _CreateGLSLShaderReplacementMaterial(const SdfPath& materialId)
{
    HdMaterialNetworkMap networkMap;
    HdMaterialNetwork&   network = networkMap.map[HdMaterialTerminalTokens->surface];

    HdMaterialNode terminal;
    terminal.identifier = _tokens->FVP_CustomBasicLightingShader;
    terminal.path = materialId.AppendChild(_tokens->FVP_CustomBasicLightingShader);
    terminal.parameters[_tokens->FVP_CustomColor] = VtValue(GfVec3f(1.0f, 0.0f, 0.0f));

    // Primvar reader so that HdGet_displayColor() is available in the shader
    HdMaterialNode primvarNode;
    primvarNode.path = materialId.AppendChild(_tokens->displayColorPrimVarReaderNode);
    primvarNode.identifier = UsdImagingTokens->UsdPrimvarReader_float3;
    primvarNode.parameters[_tokens->varname] = HdTokens->displayColor;
    primvarNode.parameters[_tokens->fallback] = VtValue(GfVec3f(0.0f, 0.0f, 1.0f));
    network.primvars.push_back(HdTokens->displayColor);

    HdMaterialRelationship primvarRel;
    primvarRel.inputId = primvarNode.path;
    primvarRel.inputName = _tokens->result;
    primvarRel.outputId = terminal.path;
    primvarRel.outputName = HdTokens->displayColor;
    network.relationships.emplace_back(std::move(primvarRel));
    network.nodes.emplace_back(std::move(primvarNode));

    networkMap.terminals.push_back(terminal.path);
    network.nodes.emplace_back(std::move(terminal));

    return VtValue(networkMap);
}

/// Command to register / unregister in the flow viewport MaterialCreationInterface a callback
/// which replaces the Maya GLSLShader materials by the FVP_CustomBasicLightingShader glslfx shader.
/// Python usage :
///   cmds.mhRegisterCustomMaterialCallback()
///   cmds.mhRegisterCustomMaterialCallback(unregister=True)
class MhRegisterCustomMaterialCallbackCmd : public MPxCommand
{
public:
    static void*   creator() { return new MhRegisterCustomMaterialCallbackCmd(); }
    static MSyntax createSyntax();

    static const MString commandName;

    MStatus doIt(const MArgList& args) override;
};

const MString MhRegisterCustomMaterialCallbackCmd::commandName("mhRegisterCustomMaterialCallback");

MSyntax MhRegisterCustomMaterialCallbackCmd::createSyntax()
{
    MSyntax syntax;
    syntax.addFlag(kUnregisterFlag, kUnregisterFlagLong);
    return syntax;
}

MStatus MhRegisterCustomMaterialCallbackCmd::doIt(const MArgList& args)
{
    MStatus      status;
    MArgDatabase argDb(syntax(), args, &status);
    if (!status) {
        return status;
    }

    auto& materialCreationInterface = Fvp::MaterialCreationInterface::Get();
    if (argDb.isFlagSet(kUnregisterFlag)) {
        materialCreationInterface.UnregisterMaterialCreationCallback();
    } else {
        materialCreationInterface.RegisterMaterialCreationCallback(_CreateGLSLShaderReplacementMaterial);
    }
    return MS::kSuccess;
}

} // namespace

MStatus registerCustomMaterialCallbackCmd(MFnPlugin& plugin)
{
    return plugin.registerCommand(
        MhRegisterCustomMaterialCallbackCmd::commandName,
        MhRegisterCustomMaterialCallbackCmd::creator,
        MhRegisterCustomMaterialCallbackCmd::createSyntax);
}

MStatus deregisterCustomMaterialCallbackCmd(MFnPlugin& plugin)
{
    // The callback lives in this plugin, it must not be called once the plugin is unloaded.
    Fvp::MaterialCreationInterface::Get().UnregisterMaterialCreationCallback();

    return plugin.deregisterCommand(MhRegisterCustomMaterialCallbackCmd::commandName);
}
