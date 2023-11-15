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

#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA_MANAGER
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA_MANAGER

//Local headers
#include "fvpViewportInformationAndSceneIndicesPerViewportData.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF {

/** Is a singleton to manage the ViewportInformationAndSceneIndicesPerViewportData which stores information and misc. scene indices data per viewport
*   So, if there are "n" Hydra viewports in the DCC, we will have "n" instances of ViewportInformationAndSceneIndicesPerViewportData.
* 
*   To get an instance of this class, please use 
*   ViewportInformationAndSceneIndicesPerViewportDataManager& manager = ViewportInformationAndSceneIndicesPerViewportDataManager:Get();
*/
class FVP_API ViewportInformationAndSceneIndicesPerViewportDataManager
{
public:

    /// Manager accessor
    static ViewportInformationAndSceneIndicesPerViewportDataManager& Get();
 
    ///A new Hydra viewport was created, this method takes the ownership of _viewportInfo which has been created on the heap and will be deleted by Flow Viewport
    void AddViewportInformation(const InformationInterface::ViewportInformation* _viewportInfo, RenderIndexProxy& renderIndexProxy);
    
    ///An Hydra viewport was deleted
    void RemoveViewportInformation(const PXR_NS::HdSceneIndexBaseRefPtr& viewportSceneIndex);

    /// Retrieve the ViewportInformation from a Hydra viewport scene index
    const InformationInterface::ViewportInformation* 
    GetViewportInformationFromViewportSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& viewportSceneIndex) const;

    const RenderIndexProxy* GetRenderIndexProxyFromViewportSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& viewportSceneIndex) const;

    const ViewportInformationAndSceneIndicesPerViewportDataSet&  GetViewportInformationAndSceneIndicesPerViewportDataSet() const {return _viewportInformationAndSceneIndicesPerViewportDataSet;}
    const ViewportInformationAndSceneIndicesPerViewportData*    GetViewportInformationAndSceneIndicesPerViewportDataFromViewportSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& viewportSceneIndex) const;
    const ViewportInformationAndSceneIndicesPerViewportData*    GetViewportInformationAndSceneIndicesPerViewportDataFromRenderIndexProxy(const RenderIndexProxy& renderIndexProxy) const;

private:
    ///Hydra viewport information
    ViewportInformationAndSceneIndicesPerViewportDataSet     _viewportInformationAndSceneIndicesPerViewportDataSet;
    
    ViewportInformationAndSceneIndicesPerViewportDataManager() = default;
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA_MANAGER