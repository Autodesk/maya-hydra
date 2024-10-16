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
#include "fvpViewportInformationAndSceneIndicesPerViewportDataManager.h"
#include "flowViewport/API/interfacesImp/fvpDataProducerSceneIndexInterfaceImp.h"
#include "flowViewport/API/interfacesImp/fvpInformationInterfaceImp.h"
#include "flowViewport/sceneIndex/fvpRenderIndexProxy.h"
#include "flowViewport/selection/fvpSelection.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndicesChainManager.h"

//Hydra headers
#include <pxr/imaging/hd/renderIndex.h>

//Std Headers
#include <mutex>

namespace 
{
    std::mutex viewportInformationAndSceneIndicesPerViewportData_mutex;
    std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr> dummyEmptyArray;

#ifdef CODE_COVERAGE_WORKAROUND
void leakViewportData(const Fvp::ViewportInformationAndSceneIndicesPerViewportDataVector& vpDataVec) {
    // Must place the leaked data vector on the heap, as a by-value
    // vector will have its destructor called at process exit, which calls
    // the vector element destructors and triggers the crash. 
    static std::vector<Fvp::ViewportInformationAndSceneIndicesPerViewportDataVector>* leakedVpData{nullptr};
    if (!leakedVpData) {
        leakedVpData = new std::vector<Fvp::ViewportInformationAndSceneIndicesPerViewportDataVector>;
    }
    leakedVpData->push_back(vpDataVec);
}
#endif

}

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

ViewportInformationAndSceneIndicesPerViewportDataManager& ViewportInformationAndSceneIndicesPerViewportDataManager::Get()
{
    static ViewportInformationAndSceneIndicesPerViewportDataManager theViewportInformationAndSceneIndicesPerViewportDataManager;
    return theViewportInformationAndSceneIndicesPerViewportDataManager;
}

//A new Hydra viewport was created
bool ViewportInformationAndSceneIndicesPerViewportDataManager::AddViewportInformation(const InformationInterface::ViewportInformation& viewportInfo, const Fvp::RenderIndexProxyPtr& renderIndexProxy, 
                                                                    const HdSceneIndexBaseRefPtr& inputSceneIndexForCustomFiltering)
{
    TF_AXIOM(renderIndexProxy && inputSceneIndexForCustomFiltering);

    ViewportInformationAndSceneIndicesPerViewportData* newElement = nullptr;

    //Add it in our array if it is not already inside
    {
        std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);

        const auto& viewportId = viewportInfo._viewportId;
        auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
        if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){
            return false;//It is already inside our array
        }

        ViewportInformationAndSceneIndicesPerViewportData temp(viewportInfo, renderIndexProxy);
        newElement = &(_viewportsInformationAndSceneIndicesPerViewportData.emplace_back(temp));
    }

    //Call this to let the data producer scene indices that apply to all viewports to be added to this new viewport as well
    const bool dataProducerSceneIndicesAdded = DataProducerSceneIndexInterfaceImp::get().hydraViewportSceneIndexAdded(viewportInfo);

    //Let the registered clients know a new viewport has been added
    InformationInterfaceImp::Get().SceneIndexAdded(viewportInfo);

    //Add the custom filtering scene indices to the merging scene index
    TF_AXIOM(newElement);
    const HdSceneIndexBaseRefPtr lastFilteringSceneIndex  = FilteringSceneIndicesChainManager::get().createFilteringSceneIndicesChain(*newElement, 
                                                                                                                                inputSceneIndexForCustomFiltering);
    //Insert the last filtering scene index into the render index
    auto renderIndex = renderIndexProxy->GetRenderIndex();
    TF_AXIOM(renderIndex);
    renderIndex->InsertSceneIndex(lastFilteringSceneIndex, SdfPath::AbsoluteRootPath());

    return dataProducerSceneIndicesAdded;
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveViewportInformation(const std::string& modelPanel)
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);
    
    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){

        InformationInterfaceImp::Get().SceneIndexRemoved(findResult->GetViewportInformation());

        const Fvp::RenderIndexProxyPtr& renderIndexProxy = findResult->GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

        if(renderIndexProxy){
            //Destroy the custom filtering scene indices chain
            auto renderIndex = renderIndexProxy->GetRenderIndex();
            const auto& filteringSceneIndex = findResult->GetLastFilteringSceneIndex();
            if (renderIndex && filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
            
        _viewportsInformationAndSceneIndicesPerViewportData.erase(findResult);
    }
}

const ViewportInformationAndSceneIndicesPerViewportData* ViewportInformationAndSceneIndicesPerViewportDataManager::GetViewportInfoAndDataFromViewportId(const std::string& viewportId)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);

    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.cbegin(), _viewportsInformationAndSceneIndicesPerViewportData.cend(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.cend()){
        const ViewportInformationAndSceneIndicesPerViewportData& data = (*findResult);
        return &data;
    }

    return nullptr;
}

ViewportInformationAndSceneIndicesPerViewportData* ViewportInformationAndSceneIndicesPerViewportDataManager::GetViewportInfoAndDataFromViewportId(const std::string& viewportId)
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);

    ViewportInformationAndSceneIndicesPerViewportDataVector::iterator findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.begin(), _viewportsInformationAndSceneIndicesPerViewportData.end(),
                    [&viewportId](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == viewportId;});
    if (findResult != _viewportsInformationAndSceneIndicesPerViewportData.end()){
        ViewportInformationAndSceneIndicesPerViewportData& data = const_cast<ViewportInformationAndSceneIndicesPerViewportData&>(*findResult);
        return &data;
    }

    return nullptr;
}

SelectionPtr ViewportInformationAndSceneIndicesPerViewportDataManager::GetOrCreateIsolateSelection(const std::string& viewportId)
{
    auto found = _isolateSelection.find(viewportId);
    if (found != _isolateSelection.end()) {
        return found->second;
    }
    // Initially isolate selection is disabled.
    _isolateSelection[viewportId] = nullptr;
    return nullptr;
}

SelectionPtr ViewportInformationAndSceneIndicesPerViewportDataManager::GetIsolateSelection(const std::string& viewportId) const
{
    auto found = _isolateSelection.find(viewportId);
    return (found != _isolateSelection.end()) ? found->second : nullptr;
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::DisableIsolateSelection(
    const std::string&  viewportId
)
{
    _isolateSelection[viewportId] = nullptr;
}

SelectionPtr
ViewportInformationAndSceneIndicesPerViewportDataManager::_EnableIsolateSelection(
    const std::string&  viewportId
)
{
    auto& is = _isolateSelection.at(viewportId);

    // If the viewport didn't have an isolate selection because it was
    // disabled, create it now.
    if (!is) {
        is = std::make_shared<Selection>();
        _isolateSelection[viewportId] = is;
    }
    return is;
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::AddIsolateSelection(
    const std::string&    viewportId, 
    const PrimSelections& primSelections
)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->AddIsolateSelection(primSelections);
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveIsolateSelection(
    const std::string&    viewportId, 
    const PrimSelections& primSelections
)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }
    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->RemoveIsolateSelection(primSelections);
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::ReplaceIsolateSelection(
    const std::string&  viewportId, 
    const SelectionPtr& isolateSelection
)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }

    _isolateSelection[viewportId] = isolateSelection;
    _isolateSelectSceneIndex->SetViewport(viewportId, isolateSelection);
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::ClearIsolateSelection(const std::string& viewportId)
{
    if (!TF_VERIFY(_isolateSelectSceneIndex, "No isolate select scene index set.")) {
        return;
    }
    _EnableIsolateSelectAndSetViewport(viewportId);
    _isolateSelectSceneIndex->ClearIsolateSelection();
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::SetIsolateSelectSceneIndex(
    const IsolateSelectSceneIndexRefPtr& sceneIndex
)
{
    _isolateSelectSceneIndex = sceneIndex;
    // If we're resetting the isolate select scene index, we're starting anew,
    // so clear out existing isolate selections.
    _isolateSelection.clear();
}

IsolateSelectSceneIndexRefPtr
ViewportInformationAndSceneIndicesPerViewportDataManager::GetIsolateSelectSceneIndex() const
{
    return _isolateSelectSceneIndex;
}

void 
ViewportInformationAndSceneIndicesPerViewportDataManager::_EnableIsolateSelectAndSetViewport(
    const std::string& viewportId
)
{
    const bool enabled{_isolateSelectSceneIndex->GetIsolateSelection()};
    auto isolateSelection = _EnableIsolateSelection(viewportId);

    // If the isolate select scene index is not set to the right viewport,
    // do a viewport switch.
    if (_isolateSelectSceneIndex->GetViewportId() != viewportId) {
        _isolateSelectSceneIndex->SetViewport(viewportId, isolateSelection);
    }
    else if (!enabled) {
        // Same viewport, so no viewport switch, but must move from disabled to
        // enabled for that viewport.
        _isolateSelectSceneIndex->SetIsolateSelection(isolateSelection);
    }
}

const std::set<PXR_NS::FVP_NS_DEF::DataProducerSceneIndexDataBaseRefPtr>&  
ViewportInformationAndSceneIndicesPerViewportDataManager::GetDataProducerSceneIndicesDataFromViewportId(const std::string& viewportId)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);

    for (const auto& viewportInformationAndSceneIndicesPerViewportData : _viewportsInformationAndSceneIndicesPerViewportData){
        const auto& viewportIdFromContainer   = viewportInformationAndSceneIndicesPerViewportData.GetViewportInformation()._viewportId;
        if (viewportIdFromContainer == viewportId){
            return viewportInformationAndSceneIndicesPerViewportData.GetDataProducerSceneIndicesData();
        }
    }

    return dummyEmptyArray;
}

bool ViewportInformationAndSceneIndicesPerViewportDataManager::ModelPanelIsAlreadyRegistered(const std::string& modelPanel)const
{
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);

    auto findResult = std::find_if(_viewportsInformationAndSceneIndicesPerViewportData.cbegin(), _viewportsInformationAndSceneIndicesPerViewportData.cend(),
                    [&modelPanel](const ViewportInformationAndSceneIndicesPerViewportData& other) { return other.GetViewportInformation()._viewportId == modelPanel;});

    return (findResult != _viewportsInformationAndSceneIndicesPerViewportData.cend());
}

void ViewportInformationAndSceneIndicesPerViewportDataManager::RemoveAllViewportsInformation()
{ 
    //Block for the lifetime of the lock
    std::lock_guard<std::mutex> lock(viewportInformationAndSceneIndicesPerViewportData_mutex);
    
    for(auto& viewportInfoAndData :_viewportsInformationAndSceneIndicesPerViewportData){

        InformationInterfaceImp::Get().SceneIndexRemoved(viewportInfoAndData.GetViewportInformation());

        const Fvp::RenderIndexProxyPtr& renderIndexProxy = viewportInfoAndData.GetRenderIndexProxy();//Get the pointer on the renderIndexProxy

        if(renderIndexProxy){
            //Destroy the custom filtering scene indices chain
            auto renderIndex = renderIndexProxy->GetRenderIndex();
            const auto& filteringSceneIndex = viewportInfoAndData.GetLastFilteringSceneIndex();
            if (renderIndex && filteringSceneIndex){
                renderIndex->RemoveSceneIndex(filteringSceneIndex);//Remove the whole chain from the render index
            }
        }
    }

#ifdef CODE_COVERAGE_WORKAROUND
    leakViewportData(_viewportsInformationAndSceneIndicesPerViewportData);
#endif

    _viewportsInformationAndSceneIndicesPerViewportData.clear();//Delete all of them

    _isolateSelection.clear();
    _isolateSelectSceneIndex = nullptr;
}

} //End of namespace FVP_NS_DEF {

