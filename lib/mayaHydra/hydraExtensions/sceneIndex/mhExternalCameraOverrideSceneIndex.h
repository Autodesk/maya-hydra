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

#ifndef MAYA_HYDRA_EXTERNAL_CAMERA_OVERRIDE_SCENE_INDEX_H
#define MAYA_HYDRA_EXTERNAL_CAMERA_OVERRIDE_SCENE_INDEX_H

// MayaHydra headers
#include "mayaHydraLib/api.h"

// Flow viewport toolkit headers
#include <flowViewport/sceneIndex/fvpSceneIndexUtils.h>

// Usd/Hydra headers
#include <pxr/imaging/hd/filteringSceneIndex.h>

namespace MAYAHYDRA_NS_DEF {

class MhExternalCameraOverrideSceneIndex;
typedef PXR_NS::TfRefPtr<MhExternalCameraOverrideSceneIndex>
    MhExternalCameraOverrideSceneIndexRefPtr;
typedef PXR_NS::TfRefPtr<const MhExternalCameraOverrideSceneIndex>
    MhExternalCameraOverrideSceneIndexConstRefPtr;

/// \class MhExternalCameraOverrideSceneIndex
///
/// A filtering scene index that overrides the camera data source on
/// renderSettings and renderProduct prims when an adskUsd:externalCamera key
/// is present in their namespacedSettings.  The external camera path is
/// sanitized ('|' -> '/', ',' stripped) and prefixed with
/// "/__adskUsd__externalCamera" before being written into the camera field.
///
class MhExternalCameraOverrideSceneIndex
    : public PXR_NS::HdSingleInputFilteringSceneIndexBase
    , public Fvp::InputSceneIndexUtils<MhExternalCameraOverrideSceneIndex>
{
public:
    using PXR_NS::HdSingleInputFilteringSceneIndexBase::_GetInputSceneIndex;

    MAYAHYDRALIB_API
    static MhExternalCameraOverrideSceneIndexRefPtr
    New(const PXR_NS::HdSceneIndexBaseRefPtr& inputSceneIndex);

    MAYAHYDRALIB_API
    PXR_NS::HdSceneIndexPrim GetPrim(const PXR_NS::SdfPath& primPath) const override;

    PXR_NS::SdfPathVector GetChildPrimPaths(const PXR_NS::SdfPath& primPath) const override
    {
        return GetInputSceneIndex()->GetChildPrimPaths(primPath);
    }

    ~MhExternalCameraOverrideSceneIndex() override = default;

protected:
    MhExternalCameraOverrideSceneIndex(
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

#endif // MAYA_HYDRA_EXTERNAL_CAMERA_OVERRIDE_SCENE_INDEX_H
