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
#ifndef FVP_CAMERA_FRAMING_H
#define FVP_CAMERA_FRAMING_H

#include <flowViewport/api.h>

#include <pxr/base/gf/matrix4d.h>
#include <pxr/base/gf/range2f.h>
#include <pxr/base/gf/vec2i.h>
#include <pxr/imaging/hd/sceneIndex.h>
#include <pxr/pxr.h>
#include <pxr/usd/sdf/path.h>

#include <optional>

namespace FVP_NS_DEF {

/// Returns the projection matrix of the perspective camera prim at \p cameraPath, as
/// HdCamera::ComputeProjectionMatrix() would compute it, without any window conforming or
/// framing. Returns nullopt if the prim is not a camera, is orthographic, or has degenerate
/// parameters.
FVP_API
std::optional<PXR_NS::GfMatrix4d> computeCameraPrimProjection(
    const PXR_NS::HdSceneIndexBaseRefPtr& sceneIndex,
    const PXR_NS::SdfPath&                cameraPath);

FVP_API
std::optional<PXR_NS::GfRange2f> solveDisplayWindow(
    const PXR_NS::GfMatrix4d& targetProj,
    const PXR_NS::GfMatrix4d& baseProj,
    const PXR_NS::GfVec2i&    dataWindow);

} // namespace FVP_NS_DEF

#endif // FVP_CAMERA_FRAMING_H
