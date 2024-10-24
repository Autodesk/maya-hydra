// Copyright 2023 Autodesk
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

#include "flowViewport/sceneIndex/fvpPathInterface.h"

#include <pxr/base/tf/diagnosticLite.h>

#include <stdexcept>

PXR_NAMESPACE_USING_DIRECTIVE

namespace FVP_NS_DEF {

PathInterface::~PathInterface() {}

SdfPath PathInterface::SceneIndexPath(const Ufe::Path& appPath) const
{
    auto primSelections = UfePathToPrimSelections(appPath);
    if (!TF_VERIFY(primSelections.size() <= 1u)) {
        throw PrimPathsCountOutOfRangeException(0, 1, primSelections.size());
    }
    return primSelections.empty() ? SdfPath() : primSelections.front().primPath;
}

SdfPathVector PathInterface::SceneIndexPaths(const Ufe::Path& appPath) const
{
    auto primSelections = UfePathToPrimSelections(appPath);

    SdfPathVector outVector;
    outVector.reserve(primSelections.size());
    for (const auto& primSelection : primSelections) {
        outVector.emplace_back(primSelection.primPath);
    }
    return outVector;
}

PrimPathsCountOutOfRangeException::PrimPathsCountOutOfRangeException(size_t min, size_t max, size_t actual)
    : std::out_of_range("Prim paths count out of range, expected [" 
        + std::to_string(min) + "," + std::to_string(max) 
        + "] but got " + std::to_string(actual))
{
}

}
