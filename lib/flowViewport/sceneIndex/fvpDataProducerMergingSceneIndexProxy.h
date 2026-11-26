// Copyright 2025 Autodesk
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

#ifndef FVP_DATA_PRODUCER_MERGING_SCENE_INDEX_PROXY_H
#define FVP_DATA_PRODUCER_MERGING_SCENE_INDEX_PROXY_H

#include "flowViewport/api.h"

#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/imaging/hd/mergingSceneIndex.h>


namespace FVP_NS_DEF {

class DataProducerMergingSceneIndexProxy;
using DataProducerMergingSceneIndexProxyPtr = std::shared_ptr<DataProducerMergingSceneIndexProxy>;

/// \class DataProducerMergingSceneIndexProxy
///
/// Class to provide a merging scene index under Flow viewport control.
///
/// The merging scene index accessed through the Hydra RenderIndex has hard-coded
/// downstream filtering scene indices.  This merging scene index proxy provides its
/// own merging scene index, after which we can easily insert downstream
/// filtering scene indices.
///
/// Inputs are reference counted: all the views of one render override share its proxy, and each
/// view inserts and removes the data producers it shows. An input is added to the merging scene
/// index by its first insertion and removed by the matching last removal.

class DataProducerMergingSceneIndexProxy
{
public:
    FVP_API
    DataProducerMergingSceneIndexProxy();

    /// Adds \p inputScene to the merging scene index, or counts one more insertion if it is
    /// already there. Every insertion must be matched by a RemoveSceneIndex().
    FVP_API
    void InsertSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputScene,
        const PXR_NS::SdfPath&                activeInputSceneRoot);

    /// Counts one less insertion of \p inputScene, and removes it from the merging scene index
    /// when none is left.
    FVP_API
    void RemoveSceneIndex(const PXR_NS::HdSceneIndexBaseRefPtr& inputScene);

    FVP_API
    PXR_NS::HdSceneIndexBaseRefPtr GetMergingSceneIndex() const;

    FVP_API
    std::set<PXR_NS::SdfPath> GetSceneRoots() const;

private:
    // The underlying merging scene index that will be used to merge the data producer scene indices
    PXR_NS::HdMergingSceneIndexRefPtr _mergingSceneIndex;

    struct _Input
    {
        PXR_NS::SdfPath sceneRoot;
        size_t          insertionCount { 0 };
    };
    std::map<PXR_NS::HdSceneIndexBaseRefPtr, _Input> _inputs;
};

}//End of namespace FVP_NS_DEF

#endif //FVP_DATA_PRODUCER_MERGING_SCENE_INDEX_PROXY_H
