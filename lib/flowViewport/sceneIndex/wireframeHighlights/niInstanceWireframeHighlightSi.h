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
#ifndef FVP_NI_INSTANCE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H
#define FVP_NI_INSTANCE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H

#include "flowViewport/api.h"
#include "flowViewport/selection/fvpSelectionFwd.h"
#include "flowViewport/sceneIndex/fvpSceneIndexUtils.h"
#include "flowViewport/fvpWireframeColorInterface.h"

#include <pxr/base/gf/vec4f.h>
#include <pxr/imaging/hd/filteringSceneIndex.h>
#include <pxr/imaging/hd/instancerTopologySchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/selectionsSchema.h>
#include <pxr/usd/sdf/path.h>

#include <functional>
#include <set>
#include <unordered_map>

namespace FVP_NS_DEF {

// Pixar declarePtrs.h TF_DECLARE_REF_PTRS macro unusable, places resulting
// type in PXR_NS.
class NiInstanceWireframeHighlightSceneIndex;
typedef PXR_NS::TfRefPtr<NiInstanceWireframeHighlightSceneIndex> NiInstanceWireframeHighlightSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const NiInstanceWireframeHighlightSceneIndex> NiInstanceWireframeHighlightSceneIndexConstRefPtr;

/// \class NiInstanceWireframeHighlightSceneIndex
///
/// Uses Hydra HdRepr to add wireframe representation to selected objects
/// and their descendants.
///
class NiInstanceWireframeHighlightSceneIndex 
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<NiInstanceWireframeHighlightSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    FVP_API
    static PXR_NS::HdSceneIndexBaseRefPtr New(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& instancePath,
        const size_t selectionIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath &primPath) const override;

    FVP_API
    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath &primPath) const override;

protected:

    FVP_API
    NiInstanceWireframeHighlightSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr&   inputSceneIndex,
        const PXR_NS::SdfPath& instancePath,
        const size_t selectionIndex,
        const std::shared_ptr<WireframeColorInterface>& wireframeColorInterface
    );

    FVP_API
    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries &entries) override;

    FVP_API
    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries &entries) override;

    FVP_API
    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase &sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries &entries) override;

private:
    const PXR_NS::SdfPath _instancePath;
    const size_t _selectionIndex;
    const std::shared_ptr<WireframeColorInterface> _wireframeColorInterface;
    
    PXR_NS::SdfPath _prototypePath;

    FVP_API
    PXR_NS::SdfPath _PrototypeNamePath() const;
};

}

#endif // FVP_NI_INSTANCE_WIREFRAME_HIGHLIGHT_SCENE_INDEX_H
