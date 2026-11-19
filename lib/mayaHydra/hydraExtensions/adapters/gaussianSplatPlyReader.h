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
#ifndef MAYAHYDRALIB_GAUSSIAN_SPLAT_PLY_READER_H
#define MAYAHYDRALIB_GAUSSIAN_SPLAT_PLY_READER_H

#include <pxr/base/gf/quatf.h>
#include <pxr/base/gf/vec3f.h>
#include <pxr/base/vt/array.h>
#include <pxr/base/vt/types.h>
#include <pxr/pxr.h>

#include <string>

PXR_NAMESPACE_OPEN_SCOPE

/// Gaussian splats decoded into the values UsdVolParticleField3DGaussianSplat expects.
struct MayaHydraGaussianSplats
{
    VtVec3fArray positions;
    /// Per-axis standard deviation; the file stores its logarithm.
    VtVec3fArray scales;
    /// Unit quaternions.
    VtQuatfArray orientations;
    /// In [0, 1]; the file stores the logit.
    VtFloatArray opacities;
    /// Degree 0 spherical harmonics coefficient (raw, as stored in the file).
    VtVec3fArray shDc;
};

/// Reads a 3D Gaussian Splatting PLY file (binary little-endian, one "vertex" element with
/// x y z, scale_0..2, rot_0..3 (w first), opacity and f_dc_0..2), decoding it the way Arnold's
/// points node does in gaussian mode. Higher-order f_rest_* coefficients are ignored.
/// Returns false and fills \p error on failure.
bool MayaHydraReadGaussianSplatPly(
    const std::string&       path,
    MayaHydraGaussianSplats& splats,
    std::string&             error);

PXR_NAMESPACE_CLOSE_SCOPE

#endif // MAYAHYDRALIB_GAUSSIAN_SPLAT_PLY_READER_H
