//
// Copyright 2026 Autodesk, Inc. All rights reserved.
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
// Material adapter for Maya image plane nodes.
//
// Maya image planes carry their texture via the "imageName" attribute rather
// than through a shading-engine / surface-shader connection like regular
// geometry.  This adapter builds a Hydra material network
// (UsdPreviewSurface + UsdUVTexture + UsdPrimvarReader_float2) that reads
// the image directly from the image plane node.
//
// Filename resolution (including animated image sequences) is delegated to
// MRenderUtil::exactImagePlaneFileName so that behaviour matches Maya's own
// image plane evaluation.  Movie/video files are detected and reported with
// a warning since Hydra does not support them.
//
#ifndef MAYAHYDRALIB_IMAGE_PLANE_MATERIAL_ADAPTER_H
#define MAYAHYDRALIB_IMAGE_PLANE_MATERIAL_ADAPTER_H

#include <mayaHydraLib/adapters/materialAdapter.h>

#include <pxr/pxr.h>

PXR_NAMESPACE_OPEN_SCOPE

class MayaHydraSceneIndex;

/// Translates a Maya imagePlane node to a Hydra material with a
/// UsdPreviewSurface + UsdUVTexture network driven by the image plane's
/// imageName attribute. Supports animated image sequences via the
/// useFrameExtension / frameExtension attributes.
class MayaHydraImagePlaneMaterialAdapter : public MayaHydraMaterialAdapter
{
public:
    MAYAHYDRALIB_API
    MayaHydraImagePlaneMaterialAdapter(
        const SdfPath&        id,
        MayaHydraSceneIndex*  mayaHydraSceneIndex,
        const MObject&        obj);

    MAYAHYDRALIB_API
    ~MayaHydraImagePlaneMaterialAdapter() override = default;

    MAYAHYDRALIB_API
    void CreateCallbacks() override;

    MAYAHYDRALIB_API
    VtValue GetMaterialResource() override;
};

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_IMAGE_PLANE_MATERIAL_ADAPTER_H
