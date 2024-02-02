//
// Copyright 2023 Autodesk, Inc. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//Local headers
#include "fvpViewportInformationAndSceneIndicesPerViewportData.h"
#include "flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"
#include "flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

ViewportInformationAndSceneIndicesPerViewportData::ViewportInformationAndSceneIndicesPerViewportData(const InformationInterface::ViewportInformation& viewportInformation, 
                                                                                                    const Fvp::RenderIndexProxyPtr& renderIndexProxy)
    : _viewportInformation(viewportInformation), _renderIndexProxy(renderIndexProxy)
{
    if (_renderIndexProxy){
        _viewportInformation._rendererName = _renderIndexProxy->GetRendererDisplayName();
    }
}

ViewportInformationAndSceneIndicesPerViewportData::~ViewportInformationAndSceneIndicesPerViewportData()
{
    DataProducerSceneIndexInterfaceImp::get().removeAllViewportDataProducerSceneIndices(*this);
    //Remove custom filtering scene indices chain
    FilteringSceneIndicesChainManager::get().destroyFilteringSceneIndicesChain(*this);
}

void ViewportInformationAndSceneIndicesPerViewportData::RemoveViewportDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex)
{
    auto findResult = std::find_if(_dataProducerSceneIndicesData.begin(), _dataProducerSceneIndicesData.end(),
        [&customDataProducerSceneIndex](const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSIData) { return dataProducerSIData->GetDataProducerSceneIndex() == customDataProducerSceneIndex;});
    if (findResult != _dataProducerSceneIndicesData.end()) {
        // Remove the data producer scene index from the merging scene index through the render proxy
        if (_renderIndexProxy){
            const PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr& dataProducerSceneIndexData = (*findResult);
            if (dataProducerSceneIndexData && dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain()) {
                    _renderIndexProxy->RemoveSceneIndex(
                        dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain());
            } else {
                TF_CODING_ERROR(
                    "dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain() is a "
                    "nullptr, that should never happen here.");
            }
        }

        // Remove the data from our records
        _dataProducerSceneIndicesData.erase(findResult); // This also decreases the ref count
    }
}

void ViewportInformationAndSceneIndicesPerViewportData::_AddAllDataProducerSceneIndexToMergingSCeneIndex()
{
    if( nullptr == _renderIndexProxy){
        return;
    }

    //Add all data producer scene index to the merging scene index through the render index proxy
    for (const auto& dataProducerSceneIndexData : _dataProducerSceneIndicesData){
        // Add the data producer scene index to the merging scene index
        if (dataProducerSceneIndexData && dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain()) {
            _renderIndexProxy->InsertSceneIndex(dataProducerSceneIndexData->GetDataProducerLastSceneIndexChain(), SdfPath::AbsoluteRootPath());
        }
    }
}

} //End of namespace FVP_NS_DEF {

