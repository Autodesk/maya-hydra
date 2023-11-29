//
// Copyright 2023 Autodesk
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
#ifndef FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_FILTERING_SCENE_INDEX_DATA_BASE_H
#define FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_FILTERING_SCENE_INDEX_DATA_BASE_H

//Flow Viewport headers
#include "flowViewport/api.h"
#include "flowViewport/API/fvpFilteringSceneIndexInterface.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/sceneIndex.h>

//The Pixar's namespace needs to be at the highest namespace level for TF_DECLARE_REF_PTRS to work.
PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

class FilteringSceneIndexDataBase;//Predeclaration
TF_DECLARE_WEAK_AND_REF_PTRS(FilteringSceneIndexDataBase);//Be able to use Ref counting pointers on FilteringSceneIndexDataBase

/** In this class, we store a filtering scene index client and all the filtering scene indices that this client has appended to a viewport, the filtering scene indices
*   could be applied to different viewports
*/
 class FVP_API FilteringSceneIndexDataBase : public TfRefBase, public TfWeakBase
{
public:

    ~FilteringSceneIndexDataBase() override = default;
    
    void updateVisibilityFromDCCNode(bool isVisible);
    ::Fvp::FilteringSceneIndexClient& getClient() {return _client;}
    bool getVisible() const{return _isVisible;}
    void setVisible(bool visible) {_isVisible = visible;}

protected:
    FilteringSceneIndexDataBase(::Fvp::FilteringSceneIndexClient& filteringSIClient);
    
    /// Filtering scene index client, not owned by this class
    ::Fvp::FilteringSceneIndexClient&  _client;

    ///_isVisible is true when the filteringSceneIndices should be visible and false when they are not such as when the hosting node has been hidden/deleted.
    bool    _isVisible = true;

private:
    FilteringSceneIndexDataBase() = default;
};

}//End of namespace FVP_NS_DEF {

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_API_PERVIEWPORTSCENEINDICESDATA_FILTERING_SCENE_INDEX_DATA_BASE_H

