//
// Copyright 2024 Autodesk
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

// Parts from
// https://github.com/Autodesk/maya-usd/blob/dev/lib/usdUfe/ufe/Utils.h

#ifndef UFEEXTENSIONS_CVT_TYPE_UTILS_H
#define UFEEXTENSIONS_CVT_TYPE_UTILS_H

#include <ufeExtensions/api.h>

#include <pxr/usd/sdf/types.h>

#include <ufe/types.h>

#include <cstring> // memcpy

namespace UfeExtensions {

//! Copy the argument matrix into the return matrix.
inline Ufe::Matrix4d toUfe(const PXR_NS::GfMatrix4d& src)
{
    Ufe::Matrix4d dst;
    std::memcpy(&dst.matrix[0][0], src.GetArray(), sizeof(double) * 16);
    return dst;
}

//! Copy the argument matrix into the return matrix.
inline PXR_NS::GfMatrix4d toUsd(const Ufe::Matrix4d& src)
{
    PXR_NS::GfMatrix4d dst;
    std::memcpy(dst.GetArray(), &src.matrix[0][0], sizeof(double) * 16);
    return dst;
}

//! Copy the argument vector into the return vector.
inline Ufe::Vector3d toUfe(const PXR_NS::GfVec3d& src)
{
    return Ufe::Vector3d(src[0], src[1], src[2]);
}

//! Copy the argument vector into the return vector.
inline PXR_NS::GfVec3d toUsd(const Ufe::Vector3d& src)
{
    return PXR_NS::GfVec3d(src.x(), src.y(), src.z());
}

} // namespace UfeExtensions

#endif
