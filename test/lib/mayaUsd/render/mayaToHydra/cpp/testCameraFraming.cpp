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
// Unit tests for Fvp::solveDisplayWindow and Fvp::computeCameraPrimProjection.

#include <flowViewport/fvpCameraFraming.h>

#include <pxr/base/gf/camera.h>
#include <pxr/base/gf/frustum.h>
#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range1f.h>
#include <pxr/base/gf/rect2i.h>
#include <pxr/base/gf/rotation.h>
#include <pxr/imaging/cameraUtil/conformWindow.h>
#include <pxr/imaging/cameraUtil/framing.h>
#include <pxr/imaging/hd/cameraSchema.h>
#include <pxr/imaging/hd/retainedDataSource.h>
#include <pxr/imaging/hd/retainedSceneIndex.h>
#include <pxr/imaging/hd/tokens.h>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>

PXR_NAMESPACE_USING_DIRECTIVE

namespace {

constexpr int kWidth = 1280;
constexpr int kHeight = 540;

const GfVec2i   kViewportSize(kWidth, kHeight);
const GfRange2f kViewportWindow(GfVec2f(0.0f), GfVec2f(kWidth, kHeight));
const GfRect2i  kViewportRect(GfVec2i(0, 0), GfVec2i(kWidth - 1, kHeight - 1));

// 4:3 film back, so it never matches the 1280x540 viewport aspect.
GfCamera makeCamera(float hOffset = 0.0f, float vOffset = 0.0f)
{
    GfCamera camera;
    camera.SetProjection(GfCamera::Perspective);
    camera.SetFocalLength(35.0f);
    camera.SetHorizontalAperture(36.0f);
    camera.SetVerticalAperture(27.0f);
    camera.SetHorizontalApertureOffset(hOffset);
    camera.SetVerticalApertureOffset(vOffset);
    camera.SetClippingRange(GfRange1f(0.1f, 1000.0f));
    return camera;
}

GfMatrix4d baseProjection(float hOffset = 0.0f, float vOffset = 0.0f)
{
    return makeCamera(hOffset, vOffset).GetFrustum().ComputeProjectionMatrix();
}

// Orthographic camera with a square film, like Maya's orthographic cameras, and a negative near
// plane, which Maya allows for orthographic cameras.
GfCamera makeOrthoCamera(float hOffset = 0.0f, float vOffset = 0.0f)
{
    GfCamera camera;
    camera.SetProjection(GfCamera::Orthographic);
    camera.SetHorizontalAperture(300.0f);
    camera.SetVerticalAperture(300.0f);
    camera.SetHorizontalApertureOffset(hOffset);
    camera.SetVerticalApertureOffset(vOffset);
    camera.SetClippingRange(GfRange1f(-100.0f, 1000.0f));
    return camera;
}

GfMatrix4d orthoBaseProjection(float hOffset = 0.0f, float vOffset = 0.0f)
{
    return makeOrthoCamera(hOffset, vOffset).GetFrustum().ComputeProjectionMatrix();
}

// Row-vector form of glOrtho(), as Maya builds its orthographic projection matrix.
GfMatrix4d glOrtho(double l, double r, double b, double t, double n, double f)
{
    GfMatrix4d m(1.0);
    m[0][0] = 2.0 / (r - l);
    m[1][1] = 2.0 / (t - b);
    m[2][2] = -2.0 / (f - n);
    m[3][0] = -(r + l) / (r - l);
    m[3][1] = -(t + b) / (t - b);
    m[3][2] = -(f + n) / (f - n);
    return m;
}

// Projection of a viewport looking through the camera, conformed by Maya's film fit.
GfMatrix4d conformedProjection(const GfMatrix4d& base, CameraUtilConformWindowPolicy policy)
{
    return CameraUtilFraming(kViewportWindow, kViewportRect).ApplyToProjectionMatrix(base, policy);
}

// Post-projection 2D zoom and pan in NDC, as applied by Maya's 2D pan/zoom and overscan.
GfMatrix4d zoomPan(double zoom, double panX, double panY)
{
    GfMatrix4d m(GfVec4d(zoom, zoom, 1.0, 1.0));
    m[3][0] = panX;
    m[3][1] = panY;
    return m;
}

GfMatrix4d applySolved(
    const GfRange2f& displayWindow,
    const GfMatrix4d& base,
    const GfRect2i&   dataWindow = kViewportRect)
{
    return CameraUtilFraming(displayWindow, dataWindow)
        .ApplyToProjectionMatrix(base, CameraUtilDontConform);
}

void expectMatrixNear(const GfMatrix4d& actual, const GfMatrix4d& expected)
{
    for (int r = 0; r < 4; ++r) {
        for (int c = 0; c < 4; ++c) {
            EXPECT_NEAR(actual[r][c], expected[r][c], 1e-4 * std::max(1.0, std::abs(expected[r][c])))
                << "at [" << r << "][" << c << "]";
        }
    }
}

void expectRoundTrip(const GfMatrix4d& target, const GfMatrix4d& base)
{
    const std::optional<GfRange2f> solved = Fvp::solveDisplayWindow(target, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    expectMatrixNear(applySolved(*solved, base), target);
}

} // namespace

// Solving the projection built from a known display window gives that display window back.
TEST(CameraFraming, recoversKnownDisplayWindow)
{
    const GfMatrix4d base = baseProjection();
    const GfRange2f  expected(GfVec2f(-150.0f, 40.0f), GfVec2f(1100.0f, 610.0f));

    const std::optional<GfRange2f> solved
        = Fvp::solveDisplayWindow(applySolved(expected, base), base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    EXPECT_NEAR(solved->GetMin()[0], expected.GetMin()[0], 1e-2);
    EXPECT_NEAR(solved->GetMin()[1], expected.GetMin()[1], 1e-2);
    EXPECT_NEAR(solved->GetMax()[0], expected.GetMax()[0], 1e-2);
    EXPECT_NEAR(solved->GetMax()[1], expected.GetMax()[1], 1e-2);
}

// When the target equals the base projection, the solved display window is the viewport.
TEST(CameraFraming, identityIsViewportWindow)
{
    const GfMatrix4d base = baseProjection();

    const std::optional<GfRange2f> solved = Fvp::solveDisplayWindow(base, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    EXPECT_NEAR(solved->GetMin()[0], 0.0f, 1e-3);
    EXPECT_NEAR(solved->GetMin()[1], 0.0f, 1e-3);
    EXPECT_NEAR(solved->GetMax()[0], float(kWidth), 1e-3);
    EXPECT_NEAR(solved->GetMax()[1], float(kHeight), 1e-3);
}

// The solved display window reproduces the projection conformed by each film fit policy.
TEST(CameraFraming, filmFitPolicies)
{
    const GfMatrix4d base = baseProjection();
    for (const CameraUtilConformWindowPolicy policy :
         { CameraUtilMatchHorizontally, CameraUtilMatchVertically, CameraUtilFit, CameraUtilCrop }) {
        SCOPED_TRACE(policy);
        expectRoundTrip(conformedProjection(base, policy), base);
    }
}

// The solved display window reproduces a projection with overscan.
TEST(CameraFraming, overscan)
{
    const GfMatrix4d base = baseProjection();
    const double     overscan = 1.3;
    expectRoundTrip(
        conformedProjection(base, CameraUtilFit) * zoomPan(1.0 / overscan, 0.0, 0.0), base);
}

// The solved display window reproduces a projection with 2D pan/zoom.
TEST(CameraFraming, panZoom)
{
    const GfMatrix4d base = baseProjection();
    expectRoundTrip(
        conformedProjection(base, CameraUtilMatchHorizontally) * zoomPan(2.5, 0.3, -0.45), base);
}

// The solved display window reproduces a projection with film offset and film translate.
TEST(CameraFraming, filmTranslate)
{
    // Film offset on the prim, plus a film translate that only Maya's matrix carries.
    const GfMatrix4d base = baseProjection(4.0f, -2.5f);
    expectRoundTrip(
        conformedProjection(base, CameraUtilMatchVertically) * zoomPan(1.0, -0.2, 0.15), base);
}

// A target projection multiplied by a constant gives the same display window, since projection
// matrices are homogeneous.
TEST(CameraFraming, targetWithDifferentWScale)
{
    const GfMatrix4d base = baseProjection();
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * zoomPan(1.4, 0.1, 0.2);

    const std::optional<GfRange2f> solved
        = Fvp::solveDisplayWindow(target * 3.0, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    expectMatrixNear(applySolved(*solved, base), target);
}

// With a render region, the display window solved for the full viewport crops the image to the
// region instead of squeezing the whole image into it.
TEST(CameraFraming, renderRegionCropsInsteadOfSqueezing)
{
    const GfMatrix4d base = baseProjection();
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * zoomPan(1.2, 0.1, -0.1);

    const std::optional<GfRange2f> solved = Fvp::solveDisplayWindow(target, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());

    // Maya's full-viewport matrix cropped to the region.
    const GfRect2i   region(GfVec2i(200, 100), GfVec2i(699, 399));
    const GfMatrix4d expected = applySolved(kViewportWindow, target, region);

    expectMatrixNear(applySolved(*solved, base, region), expected);
}

// A rotated (film roll) projection cannot be expressed by a display window, so the solve fails.
// This will be fixed when we support film roll.
TEST(CameraFraming, filmRollIsUnsolvable)
{
    const GfMatrix4d base = baseProjection();
    GfMatrix4d       roll(1.0);
    roll.SetRotate(GfRotation(GfVec3d::ZAxis(), 15.0));
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * roll;

    EXPECT_FALSE(Fvp::solveDisplayWindow(target, base, kViewportSize).has_value());
}

// A display window cannot change the projection type, so the solve fails when one projection is
// perspective and the other orthographic.
TEST(CameraFraming, projectionTypeMismatchIsUnsolvable)
{
    const GfMatrix4d orthoProj = orthoBaseProjection();
    const GfMatrix4d base = baseProjection();

    EXPECT_FALSE(Fvp::solveDisplayWindow(orthoProj, base, kViewportSize).has_value());
    EXPECT_FALSE(Fvp::solveDisplayWindow(base, orthoProj, kViewportSize).has_value());
}

// Solving an orthographic projection built from a known display window gives that display window
// back.
TEST(CameraFraming, orthographicRecoversKnownDisplayWindow)
{
    const GfMatrix4d base = orthoBaseProjection(12.0f, -7.0f);
    const GfRange2f  expected(GfVec2f(-150.0f, 40.0f), GfVec2f(1100.0f, 610.0f));

    const std::optional<GfRange2f> solved
        = Fvp::solveDisplayWindow(applySolved(expected, base), base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    EXPECT_NEAR(solved->GetMin()[0], expected.GetMin()[0], 1e-2);
    EXPECT_NEAR(solved->GetMin()[1], expected.GetMin()[1], 1e-2);
    EXPECT_NEAR(solved->GetMax()[0], expected.GetMax()[0], 1e-2);
    EXPECT_NEAR(solved->GetMax()[1], expected.GetMax()[1], 1e-2);
}

// When the orthographic target equals the base projection, the solved display window is the
// viewport.
TEST(CameraFraming, orthographicIdentityIsViewportWindow)
{
    const GfMatrix4d base = orthoBaseProjection();

    const std::optional<GfRange2f> solved = Fvp::solveDisplayWindow(base, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    EXPECT_NEAR(solved->GetMin()[0], 0.0f, 1e-3);
    EXPECT_NEAR(solved->GetMin()[1], 0.0f, 1e-3);
    EXPECT_NEAR(solved->GetMax()[0], float(kWidth), 1e-3);
    EXPECT_NEAR(solved->GetMax()[1], float(kHeight), 1e-3);
}

// The solved display window reproduces orthographic projections with each film fit policy,
// overscan, 2D pan/zoom and shake.
TEST(CameraFraming, orthographicFraming)
{
    for (const GfMatrix4d& base : { orthoBaseProjection(), orthoBaseProjection(12.0f, -7.0f) }) {
        for (const CameraUtilConformWindowPolicy policy : { CameraUtilMatchHorizontally,
                                                            CameraUtilMatchVertically,
                                                            CameraUtilFit,
                                                            CameraUtilCrop }) {
            SCOPED_TRACE(policy);
            const GfMatrix4d conformed = conformedProjection(base, policy);
            expectRoundTrip(conformed, base);
            expectRoundTrip(conformed * zoomPan(1.0 / 1.3, 0.0, 0.0), base);
            expectRoundTrip(conformed * zoomPan(2.5, 0.3, -0.45), base);
        }
    }
}

// An orthographic target multiplied by a constant gives the same display window.
TEST(CameraFraming, orthographicTargetWithDifferentWScale)
{
    const GfMatrix4d base = orthoBaseProjection();
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * zoomPan(1.4, 0.1, 0.2);

    const std::optional<GfRange2f> solved
        = Fvp::solveDisplayWindow(target * 3.0, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());
    expectMatrixNear(applySolved(*solved, base), target);
}

// The solved display window reproduces the projection Maya builds for an orthographic camera:
// a square film of orthographicWidth, fitted to the viewport, with overscan, 2D pan/zoom and
// shake applied to the window, then scaled by cameraScale.
TEST(CameraFraming, orthographicMayaProjection)
{
    const double orthoWidth = 30.0;
    const double shakeX = 1.5;
    const double shakeY = -0.5;
    const double nearPlane = -100.0;
    const double farPlane = 1000.0;

    // The camera prim publishes the square film and the shake, in world units.
    GfCamera camera;
    camera.SetProjection(GfCamera::Orthographic);
    camera.SetHorizontalAperture(float(orthoWidth / GfCamera::APERTURE_UNIT));
    camera.SetVerticalAperture(float(orthoWidth / GfCamera::APERTURE_UNIT));
    camera.SetHorizontalApertureOffset(float(shakeX / GfCamera::APERTURE_UNIT));
    camera.SetVerticalApertureOffset(float(shakeY / GfCamera::APERTURE_UNIT));
    camera.SetClippingRange(GfRange1f(float(nearPlane), float(farPlane)));
    const GfMatrix4d base = camera.GetFrustum().ComputeProjectionMatrix();

    // Maya: horizontal film fit on the wider viewport, overscan, pan/zoom, then cameraScale.
    const double aspect = double(kWidth) / kHeight;
    const double overscan = 1.2;
    const double zoom = 0.7;
    const double panX = 2.0;
    const double panY = 1.0;
    const double cameraScale = 1.5;
    const double width = orthoWidth * overscan * zoom * cameraScale;
    const double height = width / aspect;
    const double centerX = (panX + shakeX) * cameraScale;
    const double centerY = (panY + shakeY) * cameraScale;
    const GfMatrix4d target = glOrtho(
        centerX - 0.5 * width,
        centerX + 0.5 * width,
        centerY - 0.5 * height,
        centerY + 0.5 * height,
        nearPlane,
        farPlane);

    expectRoundTrip(target, base);
}

// With a render region, the orthographic display window solved for the full viewport crops the
// image to the region.
TEST(CameraFraming, orthographicRenderRegionCropsInsteadOfSqueezing)
{
    const GfMatrix4d base = orthoBaseProjection();
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * zoomPan(1.2, 0.1, -0.1);

    const std::optional<GfRange2f> solved = Fvp::solveDisplayWindow(target, base, kViewportSize);
    ASSERT_TRUE(solved.has_value());

    const GfRect2i   region(GfVec2i(200, 100), GfVec2i(699, 399));
    const GfMatrix4d expected = applySolved(kViewportWindow, target, region);

    expectMatrixNear(applySolved(*solved, base, region), expected);
}

// A rotated (film roll) orthographic projection cannot be expressed by a display window.
TEST(CameraFraming, orthographicFilmRollIsUnsolvable)
{
    const GfMatrix4d base = orthoBaseProjection();
    GfMatrix4d       roll(1.0);
    roll.SetRotate(GfRotation(GfVec3d::ZAxis(), 15.0));
    const GfMatrix4d target = conformedProjection(base, CameraUtilFit) * roll;

    EXPECT_FALSE(Fvp::solveDisplayWindow(target, base, kViewportSize).has_value());
}

// The solve fails for a viewport with zero width or height.
TEST(CameraFraming, degenerateViewportIsUnsolvable)
{
    const GfMatrix4d base = baseProjection();
    EXPECT_FALSE(Fvp::solveDisplayWindow(base, base, GfVec2i(0, kHeight)).has_value());
    EXPECT_FALSE(Fvp::solveDisplayWindow(base, base, GfVec2i(kWidth, 0)).has_value());
}

// The projection of perspective and orthographic camera prims matches GfCamera's. Non-camera
// prims, missing prims and a null scene index give no projection.
TEST(CameraFraming, cameraPrimProjection)
{
    const SdfPath perspPath("/persp");
    const SdfPath orthoPath("/ortho");
    const SdfPath notCameraPath("/notCamera");

    // HdCamera stores apertures and focal length in scene units, i.e. GfCamera units scaled by
    // GfCamera::APERTURE_UNIT and GfCamera::FOCAL_LENGTH_UNIT.
    const GfCamera camera = makeCamera(4.0f, -2.5f);
    GfCamera       orthoCamera = camera;
    orthoCamera.SetProjection(GfCamera::Orthographic);
    const auto buildCamera = [&camera](const TfToken& projection, float focalLength) {
        return HdRetainedContainerDataSource::New(
            HdCameraSchemaTokens->camera,
            HdCameraSchema::Builder()
                .SetProjection(HdRetainedTypedSampledDataSource<TfToken>::New(projection))
                .SetHorizontalAperture(HdRetainedTypedSampledDataSource<float>::New(
                    camera.GetHorizontalAperture() * GfCamera::APERTURE_UNIT))
                .SetVerticalAperture(HdRetainedTypedSampledDataSource<float>::New(
                    camera.GetVerticalAperture() * GfCamera::APERTURE_UNIT))
                .SetHorizontalApertureOffset(HdRetainedTypedSampledDataSource<float>::New(
                    camera.GetHorizontalApertureOffset() * GfCamera::APERTURE_UNIT))
                .SetVerticalApertureOffset(HdRetainedTypedSampledDataSource<float>::New(
                    camera.GetVerticalApertureOffset() * GfCamera::APERTURE_UNIT))
                .SetFocalLength(HdRetainedTypedSampledDataSource<float>::New(focalLength))
                .SetClippingRange(HdRetainedTypedSampledDataSource<GfVec2f>::New(GfVec2f(
                    camera.GetClippingRange().GetMin(), camera.GetClippingRange().GetMax())))
                .Build());
    };

    HdRetainedSceneIndexRefPtr sceneIndex = HdRetainedSceneIndex::New();
    sceneIndex->AddPrims(
        { { perspPath,
            HdPrimTypeTokens->camera,
            buildCamera(
                HdCameraSchemaTokens->perspective,
                camera.GetFocalLength() * GfCamera::FOCAL_LENGTH_UNIT) },
          // Orthographic cameras need no focal length.
          { orthoPath,
            HdPrimTypeTokens->camera,
            buildCamera(HdCameraSchemaTokens->orthographic, 0.0f) },
          { notCameraPath, HdPrimTypeTokens->mesh, HdRetainedContainerDataSource::New() } });

    const std::optional<GfMatrix4d> perspProj
        = Fvp::computeCameraPrimProjection(sceneIndex, perspPath);
    ASSERT_TRUE(perspProj.has_value());
    expectMatrixNear(*perspProj, camera.GetFrustum().ComputeProjectionMatrix());

    const std::optional<GfMatrix4d> orthoProj
        = Fvp::computeCameraPrimProjection(sceneIndex, orthoPath);
    ASSERT_TRUE(orthoProj.has_value());
    expectMatrixNear(*orthoProj, orthoCamera.GetFrustum().ComputeProjectionMatrix());

    EXPECT_FALSE(Fvp::computeCameraPrimProjection(sceneIndex, notCameraPath).has_value());
    EXPECT_FALSE(Fvp::computeCameraPrimProjection(sceneIndex, SdfPath("/missing")).has_value());
    EXPECT_FALSE(Fvp::computeCameraPrimProjection(nullptr, perspPath).has_value());
}
