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
#include "flowViewport/sceneIndex/fvpPathInterface.h"
#include "flowViewport/sceneIndex/fvpIsolateSelectSceneIndex.h"
#include "flowViewport/selection/fvpSelectionFwd.h"

//Hydra headers
#include <pxr/imaging/hd/sceneIndex.h>

namespace FVP_NS_DEF {

/** Is a singleton to manage the ViewportInformationAndSceneIndicesPerViewportData which stores information and misc. scene indices data per viewport
*   So, if there are "n" Hydra viewports in the DCC, we will have "n" instances of ViewportInformationAndSceneIndicesPerViewportData.
* 
*   To get an instance of this class, please use 
*   ViewportInformationAndSceneIndicesPerViewportDataManager& manager = ViewportInformationAndSceneIndicesPerViewportDataManager:Get();
*
*  The PerViewportDataManager also manages the per-viewport isolate selection,
*  as well as providing access to the single isolate select scene index.
*/
class FVP_API ViewportInformationAndSceneIndicesPerViewportDataManager
{
public:

    using ViewportIds = std::vector<std::string>;

    /// Manager accessor
    static ViewportInformationAndSceneIndicesPerViewportDataManager& Get();
 
    //A new Hydra viewport was created, we need inputSceneIndexForCustomFiltering to be used as an input scene index for custom filtering scene indices
    //return true if some data producer scene indices were added
    bool AddViewportInformation(const InformationInterface::ViewportInformation& viewportInfo, const Fvp::RenderIndexProxyPtr& renderIndexProxy, 
                                const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndexForCustomFiltering);
    
    //A Hydra viewport was deleted
    void RemoveViewportInformation(const std::string& modelPanel);

    const ViewportInformationAndSceneIndicesPerViewportDataVector&  GetAllViewportInfoAndData() const {return _viewportsInformationAndSceneIndicesPerViewportData;}
    ViewportInformationAndSceneIndicesPerViewportDataVector&  GetAllViewportInfoAndData() {return _viewportsInformationAndSceneIndicesPerViewportData;}

    const ViewportInformationAndSceneIndicesPerViewportData* GetViewportInfoAndDataFromViewportId(const std::string& viewportId)const;
    ViewportInformationAndSceneIndicesPerViewportData* GetViewportInfoAndDataFromViewportId(const std::string& viewportId);

    SelectionPtr GetOrCreateIsolateSelection(const std::string& viewportId);
    SelectionPtr GetIsolateSelection(const std::string& viewportId) const;
    void DisableIsolateSelection(const std::string& viewportId);

    void AddIsolateSelection(
        const std::string&    viewportId, 
        const PrimSelections& primSelections
    );
    void RemoveIsolateSelection(
        const std::string&    viewportId, 
        const PrimSelections& primSelections
    );
    void ReplaceIsolateSelection(
        const std::string&  viewportId, 
        const SelectionPtr& selection
    );
    void ClearIsolateSelection(const std::string& viewportId);

    // Get and set the isolate select scene index.  This scene index provides
    // isolate select services for all viewports.
    void SetIsolateSelectSceneIndex(
        const IsolateSelectSceneIndexRefPtr& sceneIndex
    );
    IsolateSelectSceneIndexRefPtr GetIsolateSelectSceneIndex() const;

    const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>&  GetDataProducerSceneIndicesDataFromViewportId(const std::string& viewportId)const;

    bool ModelPanelIsAlreadyRegistered(const std::string& modelPanel)const;
    void RemoveAllViewportsInformation();

private:

    // Singleton, no public creation or copy.
    ViewportInformationAndSceneIndicesPerViewportDataManager() = default;
    ViewportInformationAndSceneIndicesPerViewportDataManager(
        const ViewportInformationAndSceneIndicesPerViewportDataManager&
    ) = delete;

    SelectionPtr _EnableIsolateSelection(const std::string& viewportId);
    void _EnableIsolateSelectAndSetViewport(const std::string& viewportId);

    ///Hydra viewport information
    ViewportInformationAndSceneIndicesPerViewportDataVector     _viewportsInformationAndSceneIndicesPerViewportData;
    
    // Isolate selection, keyed by viewportId.  A null selection pointer means
    // isolate select for that viewport is disabled.  Disabling isolate select
    // on a viewport clears its isolate selection, so that at next isolate
    // select enable for that viewport its isolate selection is empty.
    std::map<std::string, SelectionPtr> _isolateSelection;

    // Isolate select scene index.
    IsolateSelectSceneIndexRefPtr _isolateSelectSceneIndex;
};

// Convenience shorthand declaration.
using ViewportDataMgr = ViewportInformationAndSceneIndicesPerViewportDataManager;

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA_MANAGER
