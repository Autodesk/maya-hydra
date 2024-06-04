//
// Copyright 2022 Pixar
//
// Licensed under the Apache License, Version 2.0 (the "Apache License")
// with the following modification; you may not use this file except in
// compliance with the Apache License and the following modification to it:
// Section 6. Trademarks. is deleted and replaced with:
//
// 6. Trademarks. This License does not grant permission to use the trade
//    names, trademarks, service marks, or product names of the Licensor
//    and its affiliates, except as required to comply with Section 4(c) of
//    the License and to reproduce the content of the NOTICE file.
//
// You may obtain a copy of the Apache License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the Apache License with the above modification is
// distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied. See the Apache License for the specific
// language governing permissions and limitations under the Apache License.
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
#ifndef FVP_SELECTION_SCENE_INDEX_H
#define FVP_SELECTION_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpPathInterface.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"

#include <pxr/imaging/hd/filteringSceneIndex.h>

#include <ufe/ufe.h>

#include <memory>

UFE_NS_DEF {
class Path;
class Selection;
}

namespace FVP_NS_DEF {

class PathInterface;
class Selection;

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class SelectionSceneIndex;
typedef PXR_NS::TfRefPtr<SelectionSceneIndex> SelectionSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const SelectionSceneIndex> SelectionSceneIndexConstRefPtr;

/// \class SelectionSceneIndex
///
/// A simple scene index adding HdSelectionsSchema to all prims selected
/// in the application.
///
class SelectionSceneIndex final
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public PathInterface
    , public Fvp::InputSceneIndexUtils<SelectionSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static SelectionSceneIndexRefPtr New(
        PXR_NS::HdSceneIndexBaseRefPtr const &inputSceneIndex,
        const std::shared_ptr<Selection>&     selection
    );

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

    /// Given a path (including usd proxy path inside a native instance) of
    /// a USD prim, determine the corresponding prim in the Usd scene index
    /// (filtered by the UsdImagingNiPrototypePropagatingSceneIndex) and
    /// populate its selections data source.
    FVP_API
    void AddSelection(const Ufe::Path& appPath);

    FVP_API
    void RemoveSelection(const Ufe::Path& appPath);

    FVP_API
    void ReplaceSelection(const Ufe::Selection& selection);

    /// Reset the scene index selection state.
    FVP_API
    void ClearSelection();

    FVP_API
    bool IsFullySelected(const PXR_NS::SdfPath& primPath) const;

    FVP_API
    bool HasFullySelectedAncestorInclusive(const PXR_NS::SdfPath& primPath) const;

    //! Path interface override.  Forwards the call to the input scene index, 
    //! and warns about empty return paths.
    //@{
    FVP_API
    PrimSelectionInfoVector ConvertUfePathToHydraSelections(const Ufe::Path& appPath) const override;
    //@}

    FVP_API
    PXR_NS::SdfPathVector GetFullySelectedPaths() const;

protected:
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;


private:
    SelectionSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr &inputSceneIndex,
        const std::shared_ptr<Selection>&     selection);

    const SelectionPtr         _selection;

    const PathInterface* const _inputSceneIndexPathInterface;
};

}

#endif
