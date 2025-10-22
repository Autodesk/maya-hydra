// Copyright 2025 Autodesk, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "shadowMatrixComputation.h"

PXR_NAMESPACE_USING_DIRECTIVE

namespace
{
// Convert GfVec4f to GfVec3f
GfVec3f makeLightPosition(const GlfSimpleLight& light)
{
    auto lightPos = light.GetPosition();
    return GfVec3f(lightPos[0], lightPos[1], lightPos[2]);
}

// Simple helper to calculate the unit length light direction - use spot direction if we have it
// TODO: In time, this will be refactored to be more robust - depend on type of light, etc
GfVec3f makeLightDirection(const GlfSimpleLight& light)
{
    GfVec3f lightDir = light.GetSpotDirection();
    if (lightDir == GfVec3f(0, 0, 0))
    {
        // if undefined, default to down
        lightDir = GfVec3f(0, -1, 0);
    }
    lightDir.Normalize();
    return lightDir;
}
} // anonymous namespace

MayaHydraShadowMatrixComputation::MayaHydraShadowMatrixComputation(
    const GfRange3f& worldBox, const GlfSimpleLight& light) :
    _worldBox { worldBox }
{
    _lightPosition      = makeLightPosition(light);
    _lightDir           = makeLightDirection(light);
    _isDirectionalLight = light.GetPosition()[3] == 0.0f;
    updateShadowMatrix(_shadowMatrix);
}

// TODO: we don't respect viewport / framing / policy changes

std::vector<GfMatrix4d> MayaHydraShadowMatrixComputation::Compute()
{
    if (_dirty)
        updateShadowMatrix(_shadowMatrix);
    return { _shadowMatrix };
}

std::vector<GfMatrix4d> MayaHydraShadowMatrixComputation::Compute(
    const GfVec4f& /* viewport */, CameraUtilConformWindowPolicy /* policy */)
{
    if (_dirty)
        updateShadowMatrix(_shadowMatrix);
    return { _shadowMatrix };
}

std::vector<GfMatrix4d> MayaHydraShadowMatrixComputation::Compute(
    const CameraUtilFraming& /* framing */, CameraUtilConformWindowPolicy /* policy */)
{
    if (_dirty)
        updateShadowMatrix(_shadowMatrix);
    return { _shadowMatrix };
}

// update and flag as dirty if the world box or light direction has changed
bool MayaHydraShadowMatrixComputation::update(const GfRange3f& worldBox, const GlfSimpleLight& light)
{
    auto lightDir      = makeLightDirection(light);
    auto lightPosition = makeLightPosition(light);

    return update(worldBox, lightDir, lightPosition);
}

// helpers to just update light or world box
bool MayaHydraShadowMatrixComputation::update(const GfRange3f& worldBox)
{
    return update(worldBox, _lightDir, _lightPosition);
}
bool MayaHydraShadowMatrixComputation::update(const GlfSimpleLight& light)
{
    return update(_worldBox, light);
}

bool MayaHydraShadowMatrixComputation::update(
    const GfRange3f& worldBox, const GfVec3f& lightDir, const GfVec3f& lightPosition)
{
    if (needsUpdate(worldBox, lightDir, lightPosition))
    {
        _worldBox      = worldBox;
        _lightDir      = lightDir;
        _lightPosition = lightPosition;

        _dirty = true;
    }
    return _dirty;
}

bool MayaHydraShadowMatrixComputation::needsUpdate(
    const GfRange3f& worldBox, const GfVec3f& lightDir, const GfVec3f& lightPosition) const
{
    auto _equalsVec3f = [](GfVec3f const& v1, GfVec3f const& v2)
    {
        constexpr auto epsilon = 1e-4f;
        return fabs(v1[0] - v2[0]) < epsilon && fabs(v1[1] - v2[1]) < epsilon &&
            fabs(v1[2] - v2[2]) < epsilon;
    };

    return !(_equalsVec3f(_lightDir, lightDir) && _equalsVec3f(_lightPosition, lightPosition) &&
        _equalsVec3f(_worldBox.GetMin(), worldBox.GetMin()) &&
        _equalsVec3f(_worldBox.GetMax(), worldBox.GetMax()));
}

void MayaHydraShadowMatrixComputation::updateShadowMatrix(GfMatrix4d& matrix)
{
    // camera
    GfFrustum frustum;

    GfVec3f pos = _lightPosition;
    if (_isDirectionalLight)
    {
        // directional light (back it up to always contain the whole scene)
        auto worldSize = (_worldBox.GetMax() - _worldBox.GetMin()).GetLength();
        // Place the light forward along its direction, looking toward the scene center
        pos = _worldBox.GetMidpoint() + (_lightDir * worldSize * 0.55f);
    }

    frustum.SetPosition(pos);

    // grow the box to include the point, then use the longest diagonal to set the near/far
    auto adjustedBox = _worldBox;
    adjustedBox.UnionWith(pos);
    auto worldSize = (adjustedBox.GetMax() - adjustedBox.GetMin()).GetLength();
    frustum.SetNearFar(GfRange1d(0.1, worldSize * 1.01));

    GfRotation rotation(
        { 0, 0, 1 }, GfVec3d(_lightPosition[0], _lightPosition[1], _lightPosition[2]));
    frustum.SetRotation(rotation);

    auto viewMatrix = frustum.ComputeViewMatrix();
    frustum.SetProjectionType(GfFrustum::Orthographic);
    GfBBox3d viewBox { _worldBox, viewMatrix };
    auto viewRange   = viewBox.ComputeAlignedRange();
    auto size        = viewRange.GetSize();
    auto half_width  = size[0] * 0.55f;
    auto half_height = size[1] * 0.55f;
    frustum.SetWindow(
        GfRange2d(GfVec2d(-half_width, -half_height), GfVec2d(half_width, half_height)));
    auto projectionMatrix = frustum.ComputeProjectionMatrix();
    matrix                = viewMatrix * projectionMatrix;

    _dirty = false;
}
