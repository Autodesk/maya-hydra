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
#ifndef FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_EXAMPLE_H
#define FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_EXAMPLE_H

//Local headers
#include "flowViewport/api.h"

//Hydra headers
#include <pxr/base/tf/declarePtrs.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>


/** This clas is an implementation of a HdSingleInputFilteringSceneIndexBase which is a filtering scene index.
*   In this example of filtering scene index we will hide the mesh primitives which have more than 10 000 vertices.
*/

//The Pixar's namespace needs to be at the highest namespace level for TF_DECLARE_WEAK_AND_REF_PTRS to work.
PXR_NAMESPACE_OPEN_SCOPE

namespace FVP_NS_DEF {

class FilteringSceneIndexExample;
TF_DECLARE_WEAK_AND_REF_PTRS(FilteringSceneIndexExample);

class FilteringSceneIndexExample : public HdSingleInputFilteringSceneIndexBase
{
public:
    using ParentClass = HdSingleInputFilteringSceneIndexBase;

    static FilteringSceneIndexExampleRefPtr New(const HdSceneIndexBaseRefPtr& inputSceneIndex){
        return TfCreateRefPtr(new FilteringSceneIndexExample(inputSceneIndex));
    }

    // From HdSceneIndexBase
    HdSceneIndexPrim GetPrim(const SdfPath& primPath) const override;//Is the useful function where we do filtering
    
    SdfPathVector GetChildPrimPaths(const SdfPath& primPath) const override{//We leave this function with no filtering for simplicity
        if (_GetInputSceneIndex()){
            return _GetInputSceneIndex()->GetChildPrimPaths(primPath);
        }

        return {};
    }
    
    ~FilteringSceneIndexExample() override = default;

protected:
    FilteringSceneIndexExample(const HdSceneIndexBaseRefPtr& inputSceneIndex) : ParentClass(inputSceneIndex) {}

    void _PrimsAdded(
        const HdSceneIndexBase&                       sender,
        const HdSceneIndexObserver::AddedPrimEntries& entries) override final
    {
        _SendPrimsAdded(entries);
    }

    void _PrimsRemoved(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::RemovedPrimEntries& entries) override 
    {
        _SendPrimsRemoved(entries);
    }

    void _PrimsDirtied(
        const HdSceneIndexBase&                         sender,
        const HdSceneIndexObserver::DirtiedPrimEntries& entries) override 
        {
            _SendPrimsDirtied(entries);
        }
};

}//end of namespace FVP_NS_DEF

PXR_NAMESPACE_CLOSE_SCOPE

#endif //FLOW_VIEWPORT_EXAMPLES_FILTERING_SCENE_INDEX_EXAMPLE_H

