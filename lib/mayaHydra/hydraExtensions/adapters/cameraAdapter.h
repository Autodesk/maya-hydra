//
// Copyright 2021 Autodesk
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
#ifndef MAYAHYDRALIB_CAMERA_ADAPTER_H
#define MAYAHYDRALIB_CAMERA_ADAPTER_H

#include <mayaHydraLib/adapters/dagAdapter.h>
#include <mayaHydraLib/adapters/shapeAdapter.h>

#include <flowViewport/fvpPurposeRenderTagsForPasses.h>

#include <pxr/pxr.h>

#include <string>
#include <unordered_set>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/**
 * \brief MayaHydraCameraAdapter is used to handle the translation from a Maya camera to hydra.
 */
class MayaHydraCameraAdapter : public MayaHydraShapeAdapter
{
public:
    MAYAHYDRALIB_API
    /// Create a camera adapter for the given DAG path.
    MayaHydraCameraAdapter(MayaHydraSceneIndex* mayaHydraSceneIndex, const MDagPath& dag);

    MAYAHYDRALIB_API
    /// Destroy the camera adapter and remove callbacks.
    virtual ~MayaHydraCameraAdapter();

    MAYAHYDRALIB_API
    /// Return whether this camera type is supported by the render index.
    bool IsSupported() const override;

    MAYAHYDRALIB_API
    /// Insert the camera prim into the scene index.
    void Populate() override;

    MAYAHYDRALIB_API
    /// Remove the camera prim from the scene index.
    void RemovePrim() override;

    MAYAHYDRALIB_API
    /// Return whether this adapter matches the given Hydra type id.
    bool HasType(const TfToken& typeId) const override;

    MAYAHYDRALIB_API
    /// Return a value for the requested Hydra data source key.
    VtValue Get(const TfToken& key) override;

    MAYAHYDRALIB_API
    /// Return a specific Hydra camera parameter value.
    VtValue GetCameraParamValue(const TfToken& paramName);

    MAYAHYDRALIB_API
    /// Register Maya callbacks for camera changes.
    void CreateCallbacks() override;

    MAYAHYDRALIB_API
    /// Return the render tag for secondary graphics.
    TfToken GetRenderTag() const override { return Fvp::secondaryGraphicsRenderTagToken; }

    /// Suppress primvar dirtying for built-in camera params that already dirty schema bits.
    bool ShouldMarkPrimvarDirtyForAttributeChange(const MPlug& plug) const override;

    /// Exposed for unit tests to verify kCameraParamAttributeNames stays in sync with GetCameraParamValue.
    MAYAHYDRALIB_API
    static const std::unordered_set<std::string>& GetCameraParamAttributeNamesForTest();

protected:
    /// Return the Hydra camera type token.
    static TfToken CameraType();
};

using MayaHydraCameraAdapterPtr = std::shared_ptr<MayaHydraCameraAdapter>;

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_CAMERA_ADAPTER_H
