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

#include "flowViewport/sceneIndex/fvpMergingSceneIndex.h"

#include "flowViewport/debugCodes.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

/* static */
MergingSceneIndexRefPtr MergingSceneIndex::New() {
    TF_DEBUG(FVP_MERGING_SCENE_INDEX)
        .Msg("MergingSceneIndex::New() called.\n");
    return TfCreateRefPtr(new MergingSceneIndex);
}

MergingSceneIndex::MergingSceneIndex() : HdMergingSceneIndex()
{
    TF_DEBUG(FVP_MERGING_SCENE_INDEX)
        .Msg("MergingSceneIndex::MergingSceneIndex() called.\n");
}

PrimSelections MergingSceneIndex::UfePathToPrimSelections(const Ufe::Path& appPath) const
{
    // FLOW_VIEWPORT_TODO  May be able to use a caching scheme for app path to
    // scene index path conversion using the run-time ID of the UFE path, as it
    // is likely that the input scene index that provided a previous answer
    // will do so again.  To be determined if the following direct approach has
    // a measurable performance impact.  PPT, 18-Sep-2023.

    // Iterate over input scene indices and ask them to convert the path if
    // they support the path interface.
    auto inputScenes = GetInputScenes();
    for (const auto& inputScene : inputScenes) {
        // Unfortunate that we have to dynamic cast, as soon as we add an input
        // scene we know whether it supports the PathInterface or not.
        auto pathInterface = dynamic_cast<const PathInterface*>(&*inputScene);
        if (pathInterface) {
            auto primSelections = pathInterface->UfePathToPrimSelections(appPath);
            if (!primSelections.empty()) {
                return primSelections;
            }
        }
    }
    return PrimSelections();
}

}
