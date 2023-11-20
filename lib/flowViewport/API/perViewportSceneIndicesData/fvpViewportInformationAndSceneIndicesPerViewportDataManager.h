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
#include "flowViewport/sceneIndex/fvpRenderIndexProxyFwd.h"

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
 
    ///A new Hydra viewport was created
    void AddViewportInformation(const InformationInterface::ViewportInformation& _viewportInfo);
    
    ///An Hydra viewport was deleted
    void RemoveViewportInformation(const std::string& modelPanel);

    const ViewportInformationAndSceneIndicesPerViewportDataSet&  GetViewportInfoAndSceneIndicesPerViewportData() const {return _viewportsInformationAndSceneIndicesPerViewportData;}

    void UpdateRenderIndexProxy(const std::string& modelPanel, const std::shared_ptr<Fvp::RenderIndexProxy>& renderIndexProxy);

private:
    ///Hydra viewport information
    ViewportInformationAndSceneIndicesPerViewportDataSet     _viewportsInformationAndSceneIndicesPerViewportData;
    
    ViewportInformationAndSceneIndicesPerViewportDataManager() = default;
};

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA_MANAGER