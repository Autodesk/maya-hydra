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
#include <pxr/imaging/hd/basisCurvesSchema.h>
#include <pxr/imaging/hd/basisCurvesTopologySchema.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/legacyDisplayStyleSchema.h>
#include <pxr/imaging/hd/lightSchema.h>
#include <pxr/imaging/hd/materialBindingSchema.h>
#include <pxr/imaging/hd/materialBindingsSchema.h>
#include <pxr/imaging/hd/materialConnectionSchema.h>
#include <pxr/imaging/hd/materialNetworkSchema.h>
#include <pxr/imaging/hd/materialNodeSchema.h>
#include <pxr/imaging/hd/materialSchema.h>
#include <pxr/imaging/hd/meshSchema.h>
#include <pxr/imaging/hd/meshTopologySchema.h>
#include <pxr/imaging/hd/primvarSchema.h>
#include <pxr/imaging/hd/primvarsSchema.h>
#include <pxr/imaging/hd/visibilitySchema.h>
#include <pxr/imaging/hd/volumeFieldSchema.h>
#include <pxr/imaging/hd/xformSchema.h>
#include <pxr/imaging/hd/extentSchema.h>

PXR_NAMESPACE_OPEN_SCOPE

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

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRASCENEINDEXUTILS_H