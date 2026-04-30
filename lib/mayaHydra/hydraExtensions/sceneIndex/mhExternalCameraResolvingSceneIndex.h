//
// Copyright 2026 Autodesk
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

#ifndef MAYA_HYDRA_EXTERNAL_CAMERA_RESOLVING_SCENE_INDEX_H
#define MAYA_HYDRA_EXTERNAL_CAMERA_RESOLVING_SCENE_INDEX_H

// MayaHydra headers
#include "mayaHydraLib/api.h"

// Flow viewport toolkit headers
#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>

// Usd/Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace MAYAHYDRA_NS_DEF {

class ExternalCameraResolvingSceneIndex;
typedef PXR_NS::TfRefPtr<ExternalCameraResolvingSceneIndex>
    ExternalCameraResolvingSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const ExternalCameraResolvingSceneIndex>
    ExternalCameraResolvingSceneIndexConstRefPtr;

/// \class ExternalCameraResolvingSceneIndex
///
/// A filtering scene index that resolves external camera paths in
/// renderSettings prims.  For each render product under
/// renderSettings.renderProducts, if the cameraPrim SdfPath contains
/// a "__adskUsd__externalCamera" component, the path prefix up to and
/// including that component is stripped, leaving the actual camera path.
///
class ExternalCameraResolvingSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<ExternalCameraResolvingSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    MAYAHYDRALIB_API
    static ExternalCameraResolvingSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    MAYAHYDRALIB_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~ExternalCameraResolvingSceneIndex() override = default;

protected:
    ExternalCameraResolvingSceneIndex(
        const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    void _PrimsAdded(
        const PXR_NS::HdSceneIndexBase&                       sender,
        const PXR_NS::HdSceneIndexObserver::AddedPrimEntries& entries) override;

    void _PrimsRemoved(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::RemovedPrimEntries& entries) override;

    void _PrimsDirtied(
        const PXR_NS::HdSceneIndexBase&                         sender,
        const PXR_NS::HdSceneIndexObserver::DirtiedPrimEntries& entries) override;
};

} // namespace MAYAHYDRA_NS_DEF

#endif // MAYA_HYDRA_EXTERNAL_CAMERA_RESOLVING_SCENE_INDEX_H
