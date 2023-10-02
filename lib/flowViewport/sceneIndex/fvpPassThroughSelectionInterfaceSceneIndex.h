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
#ifndef FVP_PASS_THROUGH_SELECTION_INTERFACE_SCENE_INDEX_H
#define FVP_PASS_THROUGH_SELECTION_INTERFACE_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/sceneIndex/fvpSelectionInterface.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace FVP_NS_DEF {

/// \class PassThroughSelectionInterfaceSceneIndexBase
///
/// Convenience base class that passes through the SelectionInterface queries
/// to its input filtering scene index.
///
class PassThroughSelectionInterfaceSceneIndexBase
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase,
    public SelectionInterface
{
public:

    //! Selection interface overrides.
    //@{
    FVP_API
    bool IsFullySelected(const PXR_NS::SdfPath& primPath) const override;

    FVP_API
    bool HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath) const override;
    //@}

protected:

    FVP_API
    PassThroughSelectionInterfaceSceneIndexBase(
        const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex);

private:

    const SelectionInterface* const _inputSceneIndexSelectionInterface;
};

}

#endif
