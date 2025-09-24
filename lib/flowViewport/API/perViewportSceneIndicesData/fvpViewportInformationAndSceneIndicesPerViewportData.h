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

#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpInformationInterface.h"
#include "flowViewport/sceneIndex/fvpDataProducerMergingSceneIndexProxy.h"
#include "fvpDataProducerSceneIndexDataBase.h"

PXR_NAMESPACE_OPEN_SCOPE
class HdRenderIndex;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

/** Stores information and misc. scene indices data per viewport
*   So, if there are "n" Hydra viewports in the DCC, we will have "n" instances of this class.
*/

class ViewportInformationAndSceneIndicesPerViewportData
{
public:
    ViewportInformationAndSceneIndicesPerViewportData(const InformationInterface::ViewportInformation& viewportInformation, 
                                                      PXR_NS::HdRenderIndex* renderIndex,
                                                      const Fvp::DataProducerMergingSceneIndexProxyPtr& dataProducerMergingSceneIndexProxy);
    ~ViewportInformationAndSceneIndicesPerViewportData();
    
    const InformationInterface::ViewportInformation& GetViewportInformation()const { return _viewportInformation;}
    PXR_NS::HdSceneIndexBaseRefPtr& GetLastFilteringSceneIndex() {return _lastFilteringSceneIndex;}
    const PXR_NS::HdSceneIndexBaseRefPtr& GetLastFilteringSceneIndex() const {return _lastFilteringSceneIndex;}
    PXR_NS::HdRenderIndex* GetRenderIndex() const { return _renderIndex; }
    const Fvp::DataProducerMergingSceneIndexProxyPtr GetDataProducerMergingSceneIndexProxy() const { return _dataProducerMergingSceneIndexProxy; }
    void SetInputSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex) {_inputSceneIndex = inputSceneIndex;}
    const PXR_NS::HdSceneIndexBaseRefPtr&   GetInputSceneIndex() const {return _inputSceneIndex;}
    const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>& GetDataProducerSceneIndicesData() const {return _dataProducerSceneIndicesData;}
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>& GetDataProducerSceneIndicesData() {return _dataProducerSceneIndicesData;}
    void RemoveViewportDataProducerSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& customDataProducerSceneIndex);

    //Needed by std::vector
    ViewportInformationAndSceneIndicesPerViewportData& operator = (const ViewportInformationAndSceneIndicesPerViewportData& other){
        _viewportInformation = other._viewportInformation;
        _dataProducerSceneIndicesData = other._dataProducerSceneIndicesData;
        _inputSceneIndex = other._inputSceneIndex;
        _lastFilteringSceneIndex = other._lastFilteringSceneIndex;
        _renderIndex = other._renderIndex;
        _dataProducerMergingSceneIndexProxy = other._dataProducerMergingSceneIndexProxy;
        return *this;
    }

private:
    ///Hydra viewport information
    InformationInterface::ViewportInformation                               _viewportInformation;
    
    ///Are the custom data producer scene indices added to this viewport
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>      _dataProducerSceneIndicesData;

    ///Is the scene index we should use as an input for the custom filtering scene indices chain
    PXR_NS::HdSceneIndexBaseRefPtr                                          _inputSceneIndex {nullptr};

    /// The last scene index of the custom filtering scene indices chain for this viewport
    PXR_NS::HdSceneIndexBaseRefPtr                                          _lastFilteringSceneIndex {nullptr};
    
    ///Is a render index per viewport
    PXR_NS::HdRenderIndex*                                                  _renderIndex { nullptr };

    /// Is a merging scene index per viewport to merge all data producer scene indices
    Fvp::DataProducerMergingSceneIndexProxyPtr                              _dataProducerMergingSceneIndexProxy { nullptr };

    /// When the render proxy is added to this class, we may have to apply all the _dataProducerSceneIndicesData to this viewport, this is what this function does.
    void _AddAllDataProducerSceneIndexToMergingSCeneIndex();
};

using ViewportInformationAndSceneIndicesPerViewportDataVector = std::vector<ViewportInformationAndSceneIndicesPerViewportData>;

} //End of namespace FVP_NS_DEF

#endif // FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_VIEWPORT_INFORMATION_AND_SCENE_INDICES_DATA_PER_VIEWPORT_DATA