//
// Copyright 2024 Autodesk, Inc. All rights reserved.
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

#ifndef MAYAHYDRASCENEINDEXUTILS_H
#define MAYAHYDRASCENEINDEXUTILS_H

#include "mayaHydraDataSource.h"

#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#if PXR_VERSION >= 2405
#include "pxr/imaging/hd/materialNodeParameterSchema.h"
#endif

PXR_NAMESPACE_OPEN_SCOPE

#if PXR_VERSION >= 2405

static
HdContainerDataSourceHandle
_ToMaterialNetworkSchema(
    const HdMaterialNetworkMap& hdNetworkMap)
{
    HD_TRACE_FUNCTION();

    TfTokenVector terminalsNames;
    std::vector<HdDataSourceBaseHandle> terminalsValues;
    std::vector<TfToken> nodeNames;
    std::vector<HdDataSourceBaseHandle> nodeValues;

    struct ParamData {
        VtValue value;
        TfToken colorSpace;
    };

    for (auto const &iter: hdNetworkMap.map) {
        const TfToken &terminalName = iter.first;
        const HdMaterialNetwork &hdNetwork = iter.second;

        if (hdNetwork.nodes.empty()) {
            continue;
        }

        terminalsNames.push_back(terminalName);

        // Transfer over individual nodes.
        // Note that the same nodes may be shared by multiple terminals.
        // We simply overwrite them here.
        for (const HdMaterialNode &node : hdNetwork.nodes) {
            std::vector<TfToken> paramsNames;
            std::vector<HdDataSourceBaseHandle> paramsValues;

            // Gather parameter value and colorspace metadata in paramsInfo, a 
            // mapping of the parameter name to its value and colorspace data.
            std::map<std::string, ParamData> paramsInfo;
            for (const auto &p : node.parameters) {

                // Strip "colorSpace" prefix 
                const std::pair<std::string, bool> res = 
                    SdfPath::StripPrefixNamespace(p.first, 
                        HdMaterialNodeParameterSchemaTokens->colorSpace);

                // Colorspace metadata
                if (res.second) {
                    paramsInfo[res.first].colorSpace = p.second.Get<TfToken>();
                }
                // Value 
                else {
                    paramsInfo[p.first].value = p.second.Get<VtValue>();
                }
            }

            // Create and store the HdMaterialNodeParameter DataSource
            for (const auto &item : paramsInfo) {
                paramsNames.push_back(TfToken(item.first));
                paramsValues.push_back(
                    HdMaterialNodeParameterSchema::Builder()
                        .SetValue(
                            HdRetainedTypedSampledDataSource<VtValue>::New(
                                item.second.value))
                        .SetColorSpace(
                            item.second.colorSpace.IsEmpty()
                            ? nullptr
                            : HdRetainedTypedSampledDataSource<TfToken>::New(
                                item.second.colorSpace))
                        .Build()
                );
            }

            // Accumulate array connections to the same input
            TfDenseHashMap<TfToken,
                TfSmallVector<HdDataSourceBaseHandle, 8>, TfToken::HashFunctor> 
                    connectionsMap;

            TfSmallVector<TfToken, 8> cNames;
            TfSmallVector<HdDataSourceBaseHandle, 8> cValues;

            for (const HdMaterialRelationship &rel : hdNetwork.relationships) {
                if (rel.outputId == node.path) {
                    TfToken outputPath = rel.inputId.GetToken(); 
                    TfToken outputName = TfToken(rel.inputName.GetString());

                    HdDataSourceBaseHandle c = 
                        HdMaterialConnectionSchema::Builder()
                            .SetUpstreamNodePath(
                                HdRetainedTypedSampledDataSource<TfToken>::New(
                                    outputPath))
                            .SetUpstreamNodeOutputName(
                                HdRetainedTypedSampledDataSource<TfToken>::New(
                                    outputName))
                            .Build();

                    connectionsMap[
                        TfToken(rel.outputName.GetString())].push_back(c);
                }
            }

            cNames.reserve(connectionsMap.size());
            cValues.reserve(connectionsMap.size());

            // NOTE: not const because HdRetainedSmallVectorDataSource needs
            //       a non-const HdDataSourceBaseHandle*
            for (auto &entryPair : connectionsMap) {
                cNames.push_back(entryPair.first);
                cValues.push_back(
                    HdRetainedSmallVectorDataSource::New(
                        entryPair.second.size(), entryPair.second.data()));
            }

            nodeNames.push_back(node.path.GetToken());
            nodeValues.push_back(
                HdMaterialNodeSchema::Builder()
                    .SetParameters(
                        HdRetainedContainerDataSource::New(
                            paramsNames.size(), 
                            paramsNames.data(),
                            paramsValues.data()))
                    .SetInputConnections(
                        HdRetainedContainerDataSource::New(
                            cNames.size(), 
                            cNames.data(),
                            cValues.data()))
                    .SetNodeIdentifier(
                        HdRetainedTypedSampledDataSource<TfToken>::New(
                            node.identifier))
                    .Build());
        }

        terminalsValues.push_back(
            HdMaterialConnectionSchema::Builder()
                .SetUpstreamNodePath(
                    HdRetainedTypedSampledDataSource<TfToken>::New(
                        hdNetwork.nodes.back().path.GetToken()))
                .SetUpstreamNodeOutputName(
                    HdRetainedTypedSampledDataSource<TfToken>::New(
                        terminalsNames.back()))
                .Build());
    }

    HdContainerDataSourceHandle nodesDefaultContext = 
        HdRetainedContainerDataSource::New(
            nodeNames.size(),
            nodeNames.data(),
            nodeValues.data());

    HdContainerDataSourceHandle terminalsDefaultContext = 
        HdRetainedContainerDataSource::New(
            terminalsNames.size(),
            terminalsNames.data(),
            terminalsValues.data());

    return HdMaterialNetworkSchema::Builder()
        .SetNodes(nodesDefaultContext)
        .SetTerminals(terminalsDefaultContext)
        .Build();
}

static bool
_ConvertHdMaterialNetworkToHdDataSources(
    const HdMaterialNetworkMap &hdNetworkMap,
    HdContainerDataSourceHandle *result)
{
    // Create the material network, potentially one per network selector
    HdDataSourceBaseHandle network = _ToMaterialNetworkSchema(hdNetworkMap);

    TfToken defaultContext = HdMaterialSchemaTokens->universalRenderContext;
    *result = HdMaterialSchema::BuildRetained(
        1, 
        &defaultContext, 
        &network);
    
    return true;
}

#else

static bool
_ConvertHdMaterialNetworkToHdDataSources(
    const HdMaterialNetworkMap &hdNetworkMap,
    HdContainerDataSourceHandle *result)
{
    HD_TRACE_FUNCTION();

    TfTokenVector terminalsNames;
    std::vector<HdDataSourceBaseHandle> terminalsValues;
    std::vector<TfToken> nodeNames;
    std::vector<HdDataSourceBaseHandle> nodeValues;

    for (auto const &iter: hdNetworkMap.map) {
        const TfToken &terminalName = iter.first;
        const HdMaterialNetwork &hdNetwork = iter.second;

        if (hdNetwork.nodes.empty()) {
            continue;
        }

        terminalsNames.push_back(terminalName);

        // Transfer over individual nodes.
        // Note that the same nodes may be shared by multiple terminals.
        // We simply overwrite them here.
        for (const HdMaterialNode &node : hdNetwork.nodes) {
            std::vector<TfToken> paramsNames;
            std::vector<HdDataSourceBaseHandle> paramsValues;

            for (const auto &p : node.parameters) {
                paramsNames.push_back(p.first);
                paramsValues.push_back(
                    HdRetainedTypedSampledDataSource<VtValue>::New(p.second)
                );
            }

            // Accumulate array connections to the same input
            TfDenseHashMap<TfToken,
                TfSmallVector<HdDataSourceBaseHandle, 8>, TfToken::HashFunctor> 
                    connectionsMap;

            TfSmallVector<TfToken, 8> cNames;
            TfSmallVector<HdDataSourceBaseHandle, 8> cValues;

            for (const HdMaterialRelationship &rel : hdNetwork.relationships) {
                if (rel.outputId == node.path) {                    
                    TfToken outputPath = rel.inputId.GetToken(); 
                    TfToken outputName = TfToken(rel.inputName.GetString());

                    HdDataSourceBaseHandle c = 
                        HdMaterialConnectionSchema::BuildRetained(
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                outputPath),
                            HdRetainedTypedSampledDataSource<TfToken>::New(
                                outputName));

                    connectionsMap[
                        TfToken(rel.outputName.GetString())].push_back(c);
                }
            }

            cNames.reserve(connectionsMap.size());
            cValues.reserve(connectionsMap.size());

            // NOTE: not const because HdRetainedSmallVectorDataSource needs
            //       a non-const HdDataSourceBaseHandle*
            for (auto &entryPair : connectionsMap) {
                cNames.push_back(entryPair.first);
                cValues.push_back(
                    HdRetainedSmallVectorDataSource::New(
                        entryPair.second.size(), entryPair.second.data()));
            }

            nodeNames.push_back(node.path.GetToken());
            nodeValues.push_back(
                HdMaterialNodeSchema::BuildRetained(
                    HdRetainedContainerDataSource::New(
                        paramsNames.size(), 
                        paramsNames.data(),
                        paramsValues.data()),
                    HdRetainedContainerDataSource::New(
                        cNames.size(), 
                        cNames.data(),
                        cValues.data()),
                    HdRetainedTypedSampledDataSource<TfToken>::New(
                        node.identifier),
                    nullptr /*renderContextNodeIdentifiers*/
                    , nullptr /* nodeTypeInfo */ 
            ));
        }

        terminalsValues.push_back(
            HdMaterialConnectionSchema::BuildRetained(
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    hdNetwork.nodes.back().path.GetToken()),
                HdRetainedTypedSampledDataSource<TfToken>::New(
                    terminalsNames.back()))
                );
    }

    HdContainerDataSourceHandle nodesDefaultContext = 
        HdRetainedContainerDataSource::New(
            nodeNames.size(), 
            nodeNames.data(),
            nodeValues.data());

    HdContainerDataSourceHandle terminalsDefaultContext = 
        HdRetainedContainerDataSource::New(
            terminalsNames.size(), 
            terminalsNames.data(),
            terminalsValues.data());

    // Create the material network, potentially one per network selector
    HdDataSourceBaseHandle network = HdMaterialNetworkSchema::BuildRetained(
        nodesDefaultContext,
        terminalsDefaultContext);
        
    TfToken defaultContext = HdMaterialSchemaTokens->universalRenderContext;
    *result = HdMaterialSchema::BuildRetained(
        1, 
        &defaultContext, 
        &network);
    
    return true;
}

#endif

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRASCENEINDEXUTILS_H
