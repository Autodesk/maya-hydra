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
#ifndef FVP_PRIM_UTILS
#define FVP_PRIM_UTILS

#include <flowViewport/api.h>

#include <pxr/pxr.h>
#include <pxr/imaging/hd/sceneIndex.h>

PXR_NAMESPACE_OPEN_SCOPE
class SdfPath;
PXR_NAMESPACE_CLOSE_SCOPE

namespace FVP_NS_DEF {

FVP_API
bool isPointInstancer(
    const PXR_NS::HdSceneIndexBaseRefPtr& si,
    const PXR_NS::SdfPath&                primPath
);

FVP_API
bool isPointInstancer(const PXR_NS::HdSceneIndexPrim& prim);

} // namespace FVP_NS_DEF

#endif // FVP_PRIM_UTILS
