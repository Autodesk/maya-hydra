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

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

ViewportInformationAndSceneIndicesPerViewportData::ViewportInformationAndSceneIndicesPerViewportData(const InformationInterface::ViewportInformation& viewportInformation)
    : _viewportInformation(viewportInformation)
{
}

void ViewportInformationAndSceneIndicesPerViewportData::SetRenderIndexProxy(RenderIndexProxy* renderIndexProxy) 
{
    _renderIndexProxy = renderIndexProxy;
    if (_renderIndexProxy){
        _viewportInformation._rendererName = _renderIndexProxy->GetRendererDisplayName();
    }
}

} //End of namespace FVP_NS_DEF {

