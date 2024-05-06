//
// Copyright 2024 Autodesk
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

//Local headers
#include "fvpBlockPrimRemovalPropagationSceneIndex.h"


/// A filtering scene index that blocks prim removal propagation. Example usage is : we are re-creating
/// the filtering scene index chain hierarchy and don't want the prim removal to propagate to the linked 
/// scene index.


namespace FVP_NS_DEF {

PXR_NAMESPACE_USING_DIRECTIVE

BlockPrimRemovalPropagationSceneIndex::BlockPrimRemovalPropagationSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex) 
    : ParentClass(inputSceneIndex), InputSceneIndexUtils(inputSceneIndex)
{
    _pathInterface = dynamic_cast<const PathInterface*>(&*GetInputSceneIndex());
    TF_AXIOM(_pathInterface);
}

void BlockPrimRemovalPropagationSceneIndex::_PrimsRemoved(const PXR_NS::HdSceneIndexBase& sender, const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries)
{
    if (!_IsObserved() || _blockPrimRemoval)return;
    _SendPrimsRemoved(entries);
}

}//end of namespace FVP_NS_DEF
