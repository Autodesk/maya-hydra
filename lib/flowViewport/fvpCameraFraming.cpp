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

#include "fvpCameraFraming.h"

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/tokens.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

constexpr double kRelativeEpsilon = 1e-5;

bool isPerspective(const GfMatrix4d& proj)
{
    const double w = std::abs(proj[2][3]);
    return w > 0.0 && std::abs(proj[3][3]) <= kRelativeEpsilon * w;
}

bool isOrthographic(const GfMatrix4d& proj)
{
    const double w = std::abs(proj[3][3]);
    return w > 0.0 && std::abs(proj[2][3]) <= kRelativeEpsilon * w;
}

float readFloat(const HdFloatDataSourceHandle& ds)
{
    return ds ? ds->GetTypedValue(0.0f) : 0.0f;
}

} // namespace

namespace FVP_NS_DEF {

// Compute the projection matrix of the camera prim. This will be used as the base projection matrix
// in solveDisplayWindow(). Here, we leverage GfCamera to use its helper methods
// GetFrustum().ComputeProjectionMatrix().
std::optional<GfMatrix4d>
computeCameraPrimProjection(const HdSceneIndexBaseRefPtr& sceneIndex, const SdfPath& cameraPath)
{
    if (!sceneIndex || cameraPath.IsEmpty()) {
        return std::nullopt;
    }

    const HdSceneIndexPrim prim = sceneIndex->GetPrim(cameraPath);
    if (prim.primType != HdPrimTypeTokens->camera) {
        return std::nullopt;
    }

    const HdCameraSchema schema = HdCameraSchema::GetFromParent(prim.dataSource);
    if (!schema.IsDefined()) {
        return std::nullopt;
    }

    bool orthographic = false;
    if (const HdTokenDataSourceHandle projDs = schema.GetProjection()) {
        orthographic = projDs->GetTypedValue(0.0f) == HdCameraSchemaTokens->orthographic;
    }

    const float horizontalAperture = readFloat(schema.GetHorizontalAperture());
    const float verticalAperture = readFloat(schema.GetVerticalAperture());
    // An orthographic projection does not depend on the focal length.
    const float focalLength = orthographic ? 1.0f : readFloat(schema.GetFocalLength());
    if (!(horizontalAperture > 0.0f) || !(verticalAperture > 0.0f) || !(focalLength > 0.0f)) {
        return std::nullopt;
    }

    // Same conversion as HdCamera::ComputeProjectionMatrix().
    GfCamera camera;
    camera.SetProjection(orthographic ? GfCamera::Orthographic : GfCamera::Perspective);
    camera.SetHorizontalAperture(horizontalAperture / GfCamera::APERTURE_UNIT);
    camera.SetVerticalAperture(verticalAperture / GfCamera::APERTURE_UNIT);
    camera.SetHorizontalApertureOffset(
        readFloat(schema.GetHorizontalApertureOffset()) / GfCamera::APERTURE_UNIT);
    camera.SetVerticalApertureOffset(
        readFloat(schema.GetVerticalApertureOffset()) / GfCamera::APERTURE_UNIT);
    camera.SetFocalLength(focalLength / GfCamera::FOCAL_LENGTH_UNIT);
    if (const HdVec2fDataSourceHandle clipDs = schema.GetClippingRange()) {
        const GfVec2f clip = clipDs->GetTypedValue(0.0f);
        camera.SetClippingRange(GfRange1f(clip[0], clip[1]));
    }

    return camera.GetFrustum().ComputeProjectionMatrix();
}

// Inverse of CameraUtilFraming::ApplyToProjectionMatrix() in USD with
// the CameraUtilDontConform policy. That method takes a base projection matrix as input
// and returns a scaled and translated projection matrix using the display window and the data
// window, where:
//   displayWindow: the film of the camera.
//   dataWindow: what the renderer draws (i.e. the panel)
//
// Our goal is to solve for the display window. We already know the dataWindow,
// the base matrix (unprocessed projection matrix from the camera in the scene) and
// the target matrix (processed projection matrix from Maya which includes overscan,
// 2D pan/zoom, film transforms, etc.). We can simply invert the equations.
//
// We have the following perspective and orthographic projection matrices
// in row-vector form, respectively:
//   [ scx   0     0     0 ]        [ scx   0     0     0 ]
//   [ 0    scy    0     0 ]        [ 0    scy    0     0 ]
//   [ stx  sty    A    -1 ]        [ 0     0     A     0 ]
//   [ 0     0     B     0 ]        [ stx  sty    B     1 ]
//
// where:
// scx/scy are influenced by the scale factors and
// stx/sty are influenced by both the scale and translation factors
// in CameraUtilFraming::ApplyToProjectionMatrix().
//
// The shift (stx, sty) sits in the row with a non-zero w term:
//   row 2 with w = -1 for perspective,
//   row 3 with w = 1 for orthographic.
// Then, in ApplyToProjectionMatrix(), we have the following equations for each term:
//   targetScaleX = baseScaleX * dispW / dataW
//   targetScaleY = baseScaleY * dispH / dataH
//   targetShiftX = (baseShiftX * dispW + w * 2 * offsetX) / dataW
//   targetShiftY = (baseShiftY * dispH - w * 2 * offsetY) / dataH
//
// The inverse equations (w = +-1, so 1 / w = w):
//   dispW = dataW * targetScaleX / baseScaleX
//   dispH = dataH * targetScaleY / baseScaleY
//   offsetX = 0.5 * w * (targetShiftX * dataW - baseShiftX * dispW)
//   offsetY = 0.5 * w * (baseShiftY * dispH - targetShiftY * dataH)
//
// For offsets, we multiply by w instead of division because w = +-1 => x / w = x * w,
// for optimization purposes.
// Note that the depth terms (A, B) are not part of the solve, depth is defined on the prim
// clippingRange.
std::optional<GfRange2f> solveDisplayWindow(
    const GfMatrix4d& targetProj,
    const GfMatrix4d& baseProj,
    const GfVec2i&    dataWindow)
{
    if (dataWindow[0] <= 0 || dataWindow[1] <= 0) {
        return std::nullopt;
    }

    // Sanity check: a display window cannot change the projection type,
    // so both matrices must have the same.
    int shiftRow = 0;
    int nonShiftOrScaleRow = 0;
    if (isPerspective(targetProj) && isPerspective(baseProj)) {
        shiftRow = 2;
        nonShiftOrScaleRow = 3;
    } else if (isOrthographic(targetProj) && isOrthographic(baseProj)) {
        shiftRow = 3;
        nonShiftOrScaleRow = 2;
    } else {
        return std::nullopt;
    }

    // Projection matrices are homogeneous, normalize them to w = +-1.
    const GfMatrix4d base = baseProj * (1.0 / std::abs(baseProj[shiftRow][3]));
    const double     w = baseProj[shiftRow][3] > 0.0 ? 1.0 : -1.0;
    const GfMatrix4d target = targetProj * (w / targetProj[shiftRow][3]);

    // A display window can only scale and move the image. Non-zero [0][1] or [1][0] terms mean
    // rotation (i.e. film roll), and non-zero [0][3], [1][3] or x/y terms in the
    // nonShiftOrScaleRow cannot be expressed by a display window. The caller warns on nullopt.
    const double scale = std::max(std::abs(target[0][0]), std::abs(target[1][1]));
    const double tolerance = kRelativeEpsilon * scale;
    if (std::abs(target[0][1]) > tolerance || std::abs(target[1][0]) > tolerance
        || std::abs(target[0][3]) > tolerance || std::abs(target[1][3]) > tolerance
        || std::abs(target[nonShiftOrScaleRow][0]) > tolerance
        || std::abs(target[nonShiftOrScaleRow][1]) > tolerance) {
        return std::nullopt;
    }

    const double dataW = dataWindow[0];
    const double dataH = dataWindow[1];

    // Avoid division by zero.
    if (base[0][0] == 0.0 || base[1][1] == 0.0) {
        return std::nullopt;
    }

    const double dispW = dataW * target[0][0] / base[0][0];
    const double dispH = dataH * target[1][1] / base[1][1];

    // We need to return the bottom-left and top-right corners of the display window
    // in GfRange2f. We can do so by computing the center and the size of the display window.
    // To compute the center, we need to find the offset and add it to the center of the data
    // window.
    const double offsetX = 0.5 * w * (target[shiftRow][0] * dataW - base[shiftRow][0] * dispW);
    const double offsetY = 0.5 * w * (base[shiftRow][1] * dispH - target[shiftRow][1] * dataH);

    const double centerX = 0.5 * dataW + offsetX;
    const double centerY = 0.5 * dataH + offsetY;

    if (!std::isfinite(dispW) || !std::isfinite(dispH) || !std::isfinite(centerX)
        || !std::isfinite(centerY) || !(dispW > 0.0) || !(dispH > 0.0)) {
        return std::nullopt;
    }

    // Return the bottom-left and top-right corners of the display window.
    return GfRange2f(
        GfVec2f(float(centerX - 0.5 * dispW), float(centerY - 0.5 * dispH)),
        GfVec2f(float(centerX + 0.5 * dispW), float(centerY + 0.5 * dispH)));
}

} // namespace FVP_NS_DEF
