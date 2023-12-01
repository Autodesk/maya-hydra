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

#ifndef FLOW_VIEWPORT_API_INTERFACESIMP_FILTERING_SCENE_INDEX_INTERFACE_IMP_H
#define FLOW_VIEWPORT_API_INTERFACESIMP_FILTERING_SCENE_INDEX_INTERFACE_IMP_H

//Local headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpFilteringSceneIndexInterface.h"//Viewport API headers
#include "flowViewport/API/fvpFilteringSceneIndexClient.h"
#include "flowViewport/API/perViewportSceneIndicesData/fvpFilteringSceneIndexDataAbstractFactory.h"

//Std headers
#include <set>

namespace FVP_NS_DEF {

///Is a singleton, use Fvp::FilteringSceneIndexInterfaceImp& filteringSceneIndexInterfaceImp = Fvp::FilteringSceneIndexInterfaceImp::Get() to get an instance of that interface
class FilteringSceneIndexInterfaceImp :  public FilteringSceneIndexInterface
{
public:
    FilteringSceneIndexInterfaceImp() = default;
    virtual ~FilteringSceneIndexInterfaceImp();

    ///Interface accessor
    static FVP_API FilteringSceneIndexInterfaceImp& get();

    //From FVP_NS_DEF::FilteringSceneIndexInterface
    bool registerFilteringSceneIndexClient(const std::shared_ptr<FilteringSceneIndexClient>& client) override;
    void unregisterFilteringSceneIndexClient(const std::shared_ptr<FilteringSceneIndexClient>& client) override;

    //Called by the DCC
    FVP_API
    void setSceneIndexDataFactory(FilteringSceneIndexDataAbstractFactory& factory);

    //Called by Flow viewport
    const std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>&           getSceneFilteringSceneIndicesData() const;
    const std::set<PXR_NS::FVP_NS_DEF::FilteringSceneIndexDataBaseRefPtr>&           getSelectionHighlightFilteringSceneIndicesData() const;

private :
    bool _CreateSceneFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client);
    bool _CreateSelectionHighlightFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client);
    void _DestroySceneFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client);
    void _DestroySelectionHighlightFilteringSceneIndicesData(const std::shared_ptr<FilteringSceneIndexClient>& client);
    
};

} //end of namespace FVP_NS_DEF


#endif // FLOW_VIEWPORT_API_INTERFACESIMP_FILTERING_SCENE_INDEX_INTERFACE_IMP_H